// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_scan_all_prints.cpp - Scan entire game file for PRINT instructions
 *
 * Brute force: scan every byte looking for 0xB2 (PRINT) or 0xB3 (PRINT_RET),
 * then decode the Z-string that follows. This should find real Zork text!
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

uint16_t decode_zstring(ZMachineState* zm, uint16_t addr) {
    uint16_t start_addr = addr;
    int alphabet = 0;
    uint32_t start_pos = zm->output_pos;  // Remember start

    while (addr < 65000 && zm->output_pos < 3900) {
        uint16_t word = read_word(zm->memory, addr);
        addr += 2;

        for (int shift = 10; shift >= 0; shift -= 5) {
            uint8_t c = (word >> shift) & 0x1F;

            if (c == 0) {
                zm->output_buffer[zm->output_pos++] = ' ';
                alphabet = 0;
            } else if (c >= 1 && c <= 3) {
                // Abbreviation - skip for now, just output [ABB]
                zm->output_buffer[zm->output_pos++] = '[';
                zm->output_buffer[zm->output_pos++] = 'A';
                zm->output_buffer[zm->output_pos++] = 'B';
                zm->output_buffer[zm->output_pos++] = 'B';
                zm->output_buffer[zm->output_pos++] = ']';
            } else if (c == 4) {
                alphabet = 1;  // Shift to uppercase
            } else if (c == 5) {
                alphabet = 2;  // Shift to punctuation
            } else if (c >= 6 && c <= 31) {
                char ch = get_alphabet_char(alphabet, c - 6);
                zm->output_buffer[zm->output_pos++] = ch;
                // Don't reset alphabet in v3 (shift lock behavior)
            }
        }

        if (word & 0x8000) break;  // End bit

        // Safety: don't decode more than 50 words
        if (addr - start_addr > 100) break;
    }

    // Check if we decoded something reasonable (at least 3 chars)
    uint32_t decoded_len = zm->output_pos - start_pos;
    if (decoded_len < 3) {
        // Reset position - probably not a real string
        zm->output_pos = start_pos;
        return 0;
    }

    return addr - start_addr;
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

    const char* header = "=== ZORK TEXT FROM BLACKHOLE RISC-V! ===\n\n";
    while (*header) zm.output_buffer[zm.output_pos++] = *header++;

    // Scan for PRINT instructions
    uint32_t prints_found = 0;
    const uint32_t MAX_PRINTS = 10;

    // Scan from address 0x5000 to end (code section)
    for (uint32_t addr = 0x5000; addr < GAME_SIZE - 10 && prints_found < MAX_PRINTS; addr++) {
        uint8_t byte = zm.memory[addr];

        if (byte == 0xB2 || byte == 0xB3) {
            // Found PRINT or PRINT_RET!
            uint32_t save_pos = zm.output_pos;

            // Try to decode Z-string
            uint16_t decoded = decode_zstring(&zm, addr + 1);

            if (decoded > 0) {
                // Successfully decoded!
                prints_found++;

                // Add newline
                zm.output_buffer[zm.output_pos++] = '\n';
                zm.output_buffer[zm.output_pos++] = '\n';
            } else {
                // Failed to decode, restore position
                zm.output_pos = save_pos;
            }
        }
    }

    const char* footer = "--- Decoded ";
    while (*footer) zm.output_buffer[zm.output_pos++] = *footer++;
    if (prints_found >= 10) zm.output_buffer[zm.output_pos++] = '0' + (prints_found / 10);
    zm.output_buffer[zm.output_pos++] = '0' + (prints_found % 10);
    const char* footer2 = " text strings from Zork! ---\n";
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
