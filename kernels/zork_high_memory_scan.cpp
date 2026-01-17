// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_high_memory_scan.cpp - Scan high memory for strings!
 *
 * According to Z-machine spec: high memory contains strings and routines.
 * Header word at $04 gives the high memory start address.
 * This is where the actual game text lives!
 */

struct ZMachineState {
    uint8_t* memory;
    char* output_buffer;
    uint32_t output_pos;
};

uint16_t read_word(uint8_t* mem, uint16_t addr) {
    return (mem[addr] << 8) | mem[addr + 1];
}

char get_alphabet_char(int set, int index) {
    if (set == 0) return 'a' + index;
    else if (set == 1) return 'A' + index;
    else {
        const char* a2 = " ^0123456789.,!?_#'\"/\\-:()";
        return (index >= 0 && index < 26) ? a2[index] : '?';
    }
}

// Try to decode a Z-string, return true if it looks valid
bool try_decode_zstring(ZMachineState* zm, uint16_t addr, uint32_t max_words) {
    uint32_t start_pos = zm->output_pos;
    int alphabet = 0;
    uint32_t words_read = 0;

    while (addr < 65000 && zm->output_pos < 3950 && words_read < max_words) {
        uint16_t word = read_word(zm->memory, addr);
        addr += 2;
        words_read++;

        for (int shift = 10; shift >= 0; shift -= 5) {
            uint8_t c = (word >> shift) & 0x1F;

            if (c == 0) {
                zm->output_buffer[zm->output_pos++] = ' ';
            } else if (c >= 1 && c <= 3) {
                // Skip abbreviations
            } else if (c == 4 || c == 5) {
                alphabet = (c == 4) ? 1 : 2;
            } else if (c >= 6 && c <= 31) {
                char ch = get_alphabet_char(alphabet, c - 6);
                zm->output_buffer[zm->output_pos++] = ch;
            }
        }

        if (word & 0x8000) {
            // End bit - check if we decoded something reasonable
            uint32_t len = zm->output_pos - start_pos;
            if (len >= 5 && len < 200) {
                // Looks valid!
                return true;
            } else {
                // Too short or too long
                zm->output_pos = start_pos;
                return false;
            }
        }
    }

    // No end bit found
    zm->output_pos = start_pos;
    return false;
}

void kernel_main() {
    uint32_t game_data_dram = get_arg_val<uint32_t>(0);
    uint32_t output_dram = get_arg_val<uint32_t>(4);

    constexpr uint32_t L1_GAME = 0x10000;
    constexpr uint32_t L1_OUTPUT = 0x50000;
    constexpr uint32_t GAME_SIZE = 86838;
    constexpr uint32_t OUTPUT_SIZE = 4096;

    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = game_data_dram,
        .page_size = GAME_SIZE
    };
    uint64_t game_noc = get_noc_addr(0, game_gen);
    noc_async_read(game_noc, L1_GAME, GAME_SIZE);
    noc_async_read_barrier();

    ZMachineState zm;
    zm.memory = (uint8_t*)L1_GAME;
    zm.output_buffer = (char*)L1_OUTPUT;
    zm.output_pos = 0;

    const char* header = "=== ZORK TEXT FROM HIGH MEMORY! ===\n\n";
    while (*header) zm.output_buffer[zm.output_pos++] = *header++;

    // Read high memory mark from header word $04
    uint16_t high_mem_mark = read_word(zm.memory, 0x04);

    const char* msg = "High memory starts at: 0x";
    while (*msg) zm.output_buffer[zm.output_pos++] = *msg++;
    const char* hex = "0123456789ABCDEF";
    zm.output_buffer[zm.output_pos++] = hex[(high_mem_mark >> 12) & 0xF];
    zm.output_buffer[zm.output_pos++] = hex[(high_mem_mark >> 8) & 0xF];
    zm.output_buffer[zm.output_pos++] = hex[(high_mem_mark >> 4) & 0xF];
    zm.output_buffer[zm.output_pos++] = hex[high_mem_mark & 0xF];
    zm.output_buffer[zm.output_pos++] = '\n';
    zm.output_buffer[zm.output_pos++] = '\n';

    // Scan high memory for valid Z-strings
    uint32_t strings_found = 0;
    const uint32_t MAX_STRINGS = 8;

    // Scan every even address (Z-strings start on word boundaries)
    for (uint32_t addr = high_mem_mark;
         addr < GAME_SIZE - 20 && strings_found < MAX_STRINGS;
         addr += 2) {

        if (try_decode_zstring(&zm, addr, 30)) {
            strings_found++;
            zm.output_buffer[zm.output_pos++] = '\n';
            zm.output_buffer[zm.output_pos++] = '\n';
        }
    }

    const char* footer = "--- Found ";
    while (*footer) zm.output_buffer[zm.output_pos++] = *footer++;
    zm.output_buffer[zm.output_pos++] = '0' + (strings_found / 10);
    zm.output_buffer[zm.output_pos++] = '0' + (strings_found % 10);
    const char* footer2 = " strings in high memory! ---\n";
    while (*footer2) zm.output_buffer[zm.output_pos++] = *footer2++;
    zm.output_buffer[zm.output_pos++] = '\0';

    InterleavedAddrGen<true> out_gen = {
        .bank_base_address = output_dram,
        .page_size = OUTPUT_SIZE
    };
    uint64_t out_noc = get_noc_addr(0, out_gen);
    noc_async_write(L1_OUTPUT, out_noc, OUTPUT_SIZE);
    noc_async_write_barrier();
}
