// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_find_prints.cpp - Execute from initial PC and find PRINT instructions!
 *
 * This is the breakthrough approach: execute the actual game code starting
 * at the initial PC (0x50D5), and when we hit PRINT instructions, decode
 * those Z-strings. THAT'S the real Zork text!
 */

struct ZMachineState {
    uint8_t* memory;
    char* output_buffer;
    uint32_t output_pos;
    uint16_t pc;
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

    while (addr < 65000 && zm->output_pos < 3800) {
        uint16_t word = read_word(zm->memory, addr);
        addr += 2;

        for (int shift = 10; shift >= 0; shift -= 5) {
            uint8_t c = (word >> shift) & 0x1F;

            if (c == 0) {
                zm->output_buffer[zm->output_pos++] = ' ';
                alphabet = 0;
            } else if (c >= 1 && c <= 3) {
                // Skip abbreviations for now
                zm->output_buffer[zm->output_pos++] = '[';
                zm->output_buffer[zm->output_pos++] = 'A';
                zm->output_buffer[zm->output_pos++] = 'B';
                zm->output_buffer[zm->output_pos++] = 'B';
                zm->output_buffer[zm->output_pos++] = ']';
            } else if (c == 4) {
                alphabet = 1;
            } else if (c == 5) {
                alphabet = 2;
            } else if (c >= 6 && c <= 31) {
                char ch = get_alphabet_char(alphabet, c - 6);
                zm->output_buffer[zm->output_pos++] = ch;
            }
        }

        if (word & 0x8000) break;
    }

    return addr - start_addr;
}

// Execute instructions looking for PRINT
// Returns: bytes to advance PC (0 = stop)
uint16_t execute_one(ZMachineState* zm, bool* found_print) {
    *found_print = false;
    uint8_t opcode_byte = zm->memory[zm->pc];

    // Check for PRINT (0xB2) or PRINT_RET (0xB3)
    if (opcode_byte == 0xB2 || opcode_byte == 0xB3) {
        *found_print = true;

        const char* msg = (opcode_byte == 0xB2) ? "\n[PRINT] " : "\n[PRINT_RET] ";
        while (*msg) zm->output_buffer[zm->output_pos++] = *msg++;

        // Decode Z-string immediately after opcode
        uint16_t bytes = decode_zstring(zm, zm->pc + 1);
        zm->output_buffer[zm->output_pos++] = '\n';

        return 1 + bytes;  // Opcode + string
    }

    // Decode instruction form to know how many bytes to skip
    if ((opcode_byte & 0xC0) == 0xC0) {
        // VAR form - variable length, skip it for now
        return 0;  // Stop - too complex without full decode
    } else if ((opcode_byte & 0x80) == 0x80) {
        // SHORT form (1OP or 0OP)
        if ((opcode_byte & 0x30) == 0x30) {
            // 0OP - 1 byte
            return 1;
        } else {
            // 1OP - 1 byte opcode + operand (1 or 2 bytes)
            // For simplicity, assume 2 bytes total
            return 2;
        }
    } else {
        // LONG form (2OP) - 3 bytes (opcode + 2 operands)
        return 3;
    }
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
    zm.pc = 0x50D5;  // Initial PC from header

    const char* header = "=== ZORK TEXT FROM BLACKHOLE! ===\n";
    while (*header) zm.output_buffer[zm.output_pos++] = *header++;

    const char* msg = "Executing from PC=0x50D5, looking for PRINT...\n";
    while (*msg) zm.output_buffer[zm.output_pos++] = *msg++;

    // Execute up to 500 instructions looking for PRINTs
    uint32_t prints_found = 0;
    const uint32_t MAX_PRINTS = 5;
    const uint32_t MAX_INSTRUCTIONS = 500;

    for (uint32_t i = 0; i < MAX_INSTRUCTIONS && prints_found < MAX_PRINTS; i++) {
        bool found_print = false;
        uint16_t advance = execute_one(&zm, &found_print);

        if (found_print) {
            prints_found++;
        }

        if (advance == 0) {
            // Hit complex instruction, stop
            break;
        }

        zm.pc += advance;

        // Safety check
        if (zm.pc >= GAME_SIZE - 10) break;
    }

    zm.output_buffer[zm.output_pos++] = '\n';
    const char* footer = "--- Found ";
    while (*footer) zm.output_buffer[zm.output_pos++] = *footer++;
    zm.output_buffer[zm.output_pos++] = '0' + (prints_found / 10);
    zm.output_buffer[zm.output_pos++] = '0' + (prints_found % 10);
    const char* footer2 = " PRINT instructions! ---\n";
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
