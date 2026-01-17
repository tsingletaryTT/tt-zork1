// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_scan_print.cpp - Scan for and decode PRINT instructions
 *
 * Instead of trying to execute instructions sequentially (which requires
 * implementing all opcodes properly), let's scan the game file for PRINT
 * instructions and decode those directly. This proves our Z-string decoder works!
 *
 * PRINT opcodes in Z-machine v3:
 * - 0xB2 (10110010) = PRINT (0OP, opcode 2)
 * - 0xB3 (10110011) = PRINT_RET (0OP, opcode 3)
 */

struct ZMachineState {
    uint8_t* memory;
    char* output_buffer;
    uint32_t output_pos;
};

// Decode Z-string (compressed text format)
// Returns: number of bytes consumed
uint16_t decode_zstring(ZMachineState* zm, uint16_t addr) {
    uint16_t pos = addr;
    uint8_t current_alphabet = 0;  // 0=lowercase, 1=uppercase, 2=special
    bool shift_next = false;
    uint8_t shift_to = 0;

    // Read words until we hit the end bit
    while (pos < 65000 && zm->output_pos < 3500) {  // Safety limits
        // Read 2-byte word (big-endian)
        uint16_t word = (zm->memory[pos] << 8) | zm->memory[pos + 1];
        pos += 2;

        // Extract 3 characters (5 bits each)
        uint8_t c0 = (word >> 10) & 0x1F;
        uint8_t c1 = (word >> 5) & 0x1F;
        uint8_t c2 = word & 0x1F;
        uint8_t chars[3] = {c0, c1, c2};

        // Decode each character
        for (uint32_t i = 0; i < 3; i++) {
            uint8_t c = chars[i];

            if (c == 0) {
                // Space character
                zm->output_buffer[zm->output_pos++] = ' ';

            } else if (c == 1) {
                // Newline
                zm->output_buffer[zm->output_pos++] = '\n';

            } else if (c == 2) {
                // Shift to A1 (uppercase) for next char
                shift_next = true;
                shift_to = 1;

            } else if (c == 3) {
                // Shift to A2 (special) for next char
                shift_next = true;
                shift_to = 2;

            } else if (c == 4) {
                // Shift lock to A1
                current_alphabet = 1;

            } else if (c == 5) {
                // Shift lock to A2
                current_alphabet = 2;

            } else if (c >= 6 && c <= 31) {
                // Regular character
                uint8_t alphabet = shift_next ? shift_to : current_alphabet;
                shift_next = false;

                char ch;
                if (alphabet == 0) {
                    // A0: lowercase a-z
                    ch = 'a' + (c - 6);
                } else if (alphabet == 1) {
                    // A1: uppercase A-Z
                    ch = 'A' + (c - 6);
                } else {
                    // A2: special characters
                    // Simplified mapping for common chars
                    const char* specials = " \n    0123456789.,!?_#'\"/\\-:()";
                    if (c >= 6 && c < 32) {
                        ch = specials[c];
                    } else {
                        ch = '?';
                    }
                }

                zm->output_buffer[zm->output_pos++] = ch;
            }
        }

        // Check end bit (bit 15)
        if (word & 0x8000) {
            break;  // Last word
        }
    }

    return pos - addr;  // Return bytes consumed
}

void kernel_main() {
    uint32_t game_data_dram = get_arg_val<uint32_t>(0);
    uint32_t game_data_size = get_arg_val<uint32_t>(1);
    uint32_t output_dram = get_arg_val<uint32_t>(4);

    // L1 memory layout
    constexpr uint32_t L1_GAME_MEMORY = 0x10000;  // 128KB for game
    constexpr uint32_t L1_OUTPUT = 0x50000;       // Output buffer
    constexpr uint32_t GAME_READ_SIZE = 86838;    // Full game file
    constexpr uint32_t OUTPUT_SIZE = 4096;

    // Read entire game into L1
    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = game_data_dram,
        .page_size = GAME_READ_SIZE
    };
    uint64_t game_noc = get_noc_addr(0, game_gen);
    noc_async_read(game_noc, L1_GAME_MEMORY, GAME_READ_SIZE);
    noc_async_read_barrier();

    // Initialize state
    ZMachineState zm;
    zm.memory = (uint8_t*)L1_GAME_MEMORY;
    zm.output_buffer = (char*)L1_OUTPUT;
    zm.output_pos = 0;

    // Write header
    const char* header = "=== SCANNING ZORK FOR TEXT ===\n\n";
    while (*header) {
        zm.output_buffer[zm.output_pos++] = *header++;
    }

    // Scan for PRINT instructions
    uint32_t prints_found = 0;
    const uint32_t MAX_PRINTS = 5;  // Show first 5 PRINT instructions

    for (uint32_t addr = 0; addr < GAME_READ_SIZE - 2 && prints_found < MAX_PRINTS; addr++) {
        uint8_t byte = zm.memory[addr];

        // Check for PRINT (0xB2) or PRINT_RET (0xB3)
        if (byte == 0xB2 || byte == 0xB3) {
            // Found a PRINT instruction!
            prints_found++;

            // Write address
            const char* hex = "0123456789ABCDEF";
            zm.output_buffer[zm.output_pos++] = '[';
            zm.output_buffer[zm.output_pos++] = '0';
            zm.output_buffer[zm.output_pos++] = 'x';
            zm.output_buffer[zm.output_pos++] = hex[(addr >> 12) & 0xF];
            zm.output_buffer[zm.output_pos++] = hex[(addr >> 8) & 0xF];
            zm.output_buffer[zm.output_pos++] = hex[(addr >> 4) & 0xF];
            zm.output_buffer[zm.output_pos++] = hex[addr & 0xF];
            zm.output_buffer[zm.output_pos++] = ']';
            zm.output_buffer[zm.output_pos++] = ' ';

            if (byte == 0xB2) {
                const char* msg = "PRINT: ";
                while (*msg) zm.output_buffer[zm.output_pos++] = *msg++;
            } else {
                const char* msg = "PRINT_RET: ";
                while (*msg) zm.output_buffer[zm.output_pos++] = *msg++;
            }

            // Decode Z-string immediately after opcode
            decode_zstring(&zm, addr + 1);

            zm.output_buffer[zm.output_pos++] = '\n';
            zm.output_buffer[zm.output_pos++] = '\n';
        }
    }

    // Summary
    const char* footer = "--- Found ";
    while (*footer) zm.output_buffer[zm.output_pos++] = *footer++;
    zm.output_buffer[zm.output_pos++] = '0' + (prints_found / 10);
    zm.output_buffer[zm.output_pos++] = '0' + (prints_found % 10);
    const char* footer2 = " PRINT instructions! ---\n";
    while (*footer2) zm.output_buffer[zm.output_pos++] = *footer2++;

    zm.output_buffer[zm.output_pos++] = '\0';

    // Write output to DRAM
    InterleavedAddrGen<true> out_gen = {
        .bank_base_address = output_dram,
        .page_size = OUTPUT_SIZE
    };
    uint64_t out_noc = get_noc_addr(0, out_gen);
    noc_async_write(L1_OUTPUT, out_noc, OUTPUT_SIZE);
    noc_async_write_barrier();
}
