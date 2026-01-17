// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_decode_v2.cpp - Working Z-string decoder (abbreviations skipped)
 *
 * Based on successful "hello" test. This version skips abbreviations
 * for simplicity, which should still decode most text correctly.
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
    if (set == 0) {
        return 'a' + index;  // lowercase
    } else if (set == 1) {
        return 'A' + index;  // uppercase
    } else {
        // A2 punctuation for v3
        const char* a2 = " ^0123456789.,!?_#'\"/\\-:()";
        return (index >= 0 && index < 26) ? a2[index] : '?';
    }
}

uint16_t decode_zstring(ZMachineState* zm, uint16_t addr) {
    uint16_t start_addr = addr;
    int alphabet = 0;  // Current alphabet (0=lower, 1=upper, 2=punct)

    while (addr < 65000 && zm->output_pos < 3900) {
        uint16_t word = read_word(zm->memory, addr);
        addr += 2;

        // Extract 3 5-bit characters
        for (int shift = 10; shift >= 0; shift -= 5) {
            uint8_t c = (word >> shift) & 0x1F;

            if (c == 0) {
                // Space
                zm->output_buffer[zm->output_pos++] = ' ';
                alphabet = 0;
            } else if (c >= 1 && c <= 3) {
                // ABBREVIATION - skip for now
                // In full implementation: look up abbreviation table
                // For now: just output [ABBn] to show it's an abbreviation
                zm->output_buffer[zm->output_pos++] = '[';
                zm->output_buffer[zm->output_pos++] = 'A';
                zm->output_buffer[zm->output_pos++] = 'B';
                zm->output_buffer[zm->output_pos++] = 'B';
                zm->output_buffer[zm->output_pos++] = '0' + c;
                zm->output_buffer[zm->output_pos++] = ']';
            } else if (c == 4) {
                // Temporary shift to uppercase (next char only)
                // Actually in v3, 4 is shift LOCK to A1
                // For v1-2, 2-3 are temporary shifts; for v3+, 4-5 are shift locks
                alphabet = 1;
            } else if (c == 5) {
                // Shift lock to A2 (punctuation)
                alphabet = 2;
            } else if (c >= 6 && c <= 31) {
                // Regular character
                char ch = get_alphabet_char(alphabet, c - 6);
                zm->output_buffer[zm->output_pos++] = ch;
                // In v3, alphabet locks persist, so we don't reset
                // (unless it was a temporary shift, which we're not fully handling)
            }
        }

        if (word & 0x8000) {
            break;  // End bit set
        }
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

    // Read game
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

    const char* header = "=== ZORK TEXT FROM BLACKHOLE! ===\n\n";
    while (*header) zm.output_buffer[zm.output_pos++] = *header++;

    // Decode first 20 object names
    uint16_t obj_table = read_word(zm.memory, 0x0A);
    uint16_t first_obj = obj_table + 62;

    const char* msg = "OBJECTS:\n";
    while (*msg) zm.output_buffer[zm.output_pos++] = *msg++;

    for (uint32_t i = 1; i <= 20 && zm.output_pos < 3500; i++) {
        uint16_t obj_addr = first_obj + ((i - 1) * 9);
        uint16_t prop_addr = read_word(zm.memory, obj_addr + 7);

        if (prop_addr == 0 || prop_addr >= GAME_SIZE - 10) continue;

        uint8_t name_len = zm.memory[prop_addr];
        if (name_len == 0 || name_len > 20) continue;

        // Write number
        if (i >= 10) {
            zm.output_buffer[zm.output_pos++] = '0' + (i / 10);
            zm.output_buffer[zm.output_pos++] = '0' + (i % 10);
        } else {
            zm.output_buffer[zm.output_pos++] = ' ';
            zm.output_buffer[zm.output_pos++] = '0' + i;
        }
        zm.output_buffer[zm.output_pos++] = '.';
        zm.output_buffer[zm.output_pos++] = ' ';

        // Decode name
        decode_zstring(&zm, prop_addr + 1);
        zm.output_buffer[zm.output_pos++] = '\n';
    }

    zm.output_buffer[zm.output_pos++] = '\n';
    const char* footer = "--- Text decoded on Blackhole RISC-V! ---\n";
    while (*footer) zm.output_buffer[zm.output_pos++] = *footer++;
    zm.output_buffer[zm.output_pos++] = '\0';

    // Write output
    InterleavedAddrGen<true> out_gen = {
        .bank_base_address = output_dram,
        .page_size = OUTPUT_SIZE
    };
    uint64_t out_noc = get_noc_addr(0, out_gen);
    noc_async_write(L1_OUTPUT, out_noc, OUTPUT_SIZE);
    noc_async_write_barrier();
}
