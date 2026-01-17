// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_execute.cpp - Execute Z-machine instructions!
 *
 * Architecture designed for future HTTP/LLM integration:
 * - Z-machine core: Execute bytecode (this kernel)
 * - I/O abstraction: DRAM buffers (now) → HTTP to vLLM (later)
 * - Text output: Written to DRAM buffer (later: HTTP response)
 * - Command input: Read from DRAM buffer (later: HTTP request to LLM)
 *
 * Phase 1 (NOW): Execute instructions, output to DRAM
 * Phase 2 (LATER): Replace I/O with HTTP calls to other Blackhole chips
 * Phase 3 (BONUS): Third chip runs autonomous LLM player
 */

// Simple Z-machine state (minimal for now)
struct ZMachineState {
    uint8_t* memory;           // Pointer to game data in L1
    uint16_t pc;               // Program counter
    uint16_t stack[1024];      // Call stack
    uint16_t stack_ptr;        // Stack pointer
    uint8_t locals[16];        // Local variables
    uint16_t globals_addr;     // Address of global variables
    char* output_buffer;       // Where to write text (FUTURE: HTTP endpoint)
    uint32_t output_pos;       // Current position in output
};

// Decode Z-string (compressed text format)
// Z-strings use 5-bit encoding with 3 alphabets
// Returns: number of bytes consumed
uint16_t decode_zstring(ZMachineState* zm, uint16_t addr) {
    // Z-machine v3 alphabets
    const char* A0 = "      \n  0123456789.,!?_#'\"/\\-:()";  // alphabet 0 (lowercase implied)
    const char* A1 = "      \n  0123456789.,!?_#'\"/\\-:()";  // alphabet 1 (uppercase implied)
    const char* A2 = " \n    0123456789.,!?_#'\"/\\-:()";     // alphabet 2 (special)

    uint16_t pos = addr;
    uint8_t current_alphabet = 0;  // Start in A0 (lowercase)
    bool shift_next = false;
    uint8_t shift_to = 0;

    // Read words until we hit the end bit
    while (pos < 65000 && zm->output_pos < 4000) {  // Safety limits
        // Read 2-byte word (big-endian)
        uint16_t word = (zm->memory[pos] << 8) | zm->memory[pos + 1];
        pos += 2;

        // Extract 3 characters (5 bits each)
        uint8_t chars[3];
        chars[0] = (word >> 10) & 0x1F;
        chars[1] = (word >> 5) & 0x1F;
        chars[2] = word & 0x1F;

        // Decode each character
        for (uint32_t i = 0; i < 3; i++) {
            uint8_t c = chars[i];

            if (c == 0) {
                // Space character
                zm->output_buffer[zm->output_pos++] = ' ';
                current_alphabet = 0;  // Reset to A0

            } else if (c == 1) {
                // Newline (in v3)
                zm->output_buffer[zm->output_pos++] = '\n';
                current_alphabet = 0;

            } else if (c == 2 || c == 3) {
                // Shift to next character
                shift_next = true;
                shift_to = (c == 2) ? 1 : 2;

            } else if (c == 4 || c == 5) {
                // Shift lock (v3+)
                current_alphabet = (c == 4) ? 1 : 2;

            } else if (c >= 6 && c <= 31) {
                // Regular character
                uint8_t alphabet = shift_next ? shift_to : current_alphabet;
                shift_next = false;

                char ch;
                if (alphabet == 0) {
                    // A0: lowercase letters
                    if (c >= 6 && c <= 31) {
                        ch = 'a' + (c - 6);
                    } else {
                        ch = '?';
                    }
                } else if (alphabet == 1) {
                    // A1: uppercase letters
                    if (c >= 6 && c <= 31) {
                        ch = 'A' + (c - 6);
                    } else {
                        ch = '?';
                    }
                } else {
                    // A2: special characters
                    if (c >= 6 && c < 32) {
                        ch = A2[c];
                    } else {
                        ch = '?';
                    }
                }

                zm->output_buffer[zm->output_pos++] = ch;

                // Reset alphabet if we shifted
                if (alphabet != current_alphabet) {
                    // Don't reset, shift was temporary
                }
            }
        }

        // Check end bit (bit 15)
        if (word & 0x8000) {
            break;  // Last word
        }
    }

    return pos - addr;  // Return bytes consumed
}

// Execute one instruction
// Returns: 0 = continue, 1 = quit, 2 = need more data (FUTURE: make HTTP call)
uint32_t execute_instruction(ZMachineState* zm) {
    uint8_t opcode_byte = zm->memory[zm->pc];

    // Decode instruction form
    if ((opcode_byte & 0xC0) == 0xC0) {
        // VAR form (variable operands)
        uint8_t opcode = opcode_byte & 0x1F;
        zm->pc++;

        const char* msg = "[VAR instruction: ";
        while (*msg) {
            zm->output_buffer[zm->output_pos++] = *msg++;
        }
        zm->output_buffer[zm->output_pos++] = '0' + (opcode / 10);
        zm->output_buffer[zm->output_pos++] = '0' + (opcode % 10);
        zm->output_buffer[zm->output_pos++] = ']';
        zm->output_buffer[zm->output_pos++] = '\n';

    } else if ((opcode_byte & 0x80) == 0x80) {
        // SHORT form (1OP or 0OP)

        if ((opcode_byte & 0x30) == 0x30) {
            // 0OP (no operands)
            uint8_t opcode = opcode_byte & 0x0F;
            zm->pc++;

            // Check for PRINT (0xB2) or PRINT_RET (0xB3)
            if (opcode == 0x02) {
                // PRINT - print literal string
                const char* msg = "\n[PRINT] ";
                while (*msg) {
                    zm->output_buffer[zm->output_pos++] = *msg++;
                }

                // String follows immediately after opcode
                uint16_t bytes_consumed = decode_zstring(zm, zm->pc);
                zm->pc += bytes_consumed;

            } else if (opcode == 0x03) {
                // PRINT_RET - print and return
                const char* msg = "\n[PRINT_RET] ";
                while (*msg) {
                    zm->output_buffer[zm->output_pos++] = *msg++;
                }
                uint16_t bytes_consumed = decode_zstring(zm, zm->pc);
                zm->pc += bytes_consumed;
                zm->output_buffer[zm->output_pos++] = '\n';
                return 1;  // Return from routine

            } else if (opcode == 0x0B) {
                // NEW_LINE
                zm->output_buffer[zm->output_pos++] = '\n';

            } else {
                // Other 0OP
                const char* msg = "[0OP: ";
                while (*msg) {
                    zm->output_buffer[zm->output_pos++] = *msg++;
                }
                zm->output_buffer[zm->output_pos++] = '0' + (opcode / 10);
                zm->output_buffer[zm->output_pos++] = '0' + (opcode % 10);
                zm->output_buffer[zm->output_pos++] = ']';
                zm->output_buffer[zm->output_pos++] = '\n';
            }
        } else {
            // 1OP (one operand)
            zm->pc += 2;  // Skip opcode + operand
        }

    } else {
        // LONG form (2OP)
        uint8_t opcode = opcode_byte & 0x1F;
        zm->pc += 3;  // Skip opcode + 2 operands

        const char* msg = "[2OP: ";
        while (*msg) {
            zm->output_buffer[zm->output_pos++] = *msg++;
        }
        zm->output_buffer[zm->output_pos++] = '0' + (opcode / 10);
        zm->output_buffer[zm->output_pos++] = '0' + (opcode % 10);
        zm->output_buffer[zm->output_pos++] = ']';
        zm->output_buffer[zm->output_pos++] = '\n';
    }

    return 0;  // Continue execution
}

void kernel_main() {
    uint32_t game_data_dram = get_arg_val<uint32_t>(0);
    uint32_t game_data_size = get_arg_val<uint32_t>(1);
    uint32_t output_dram = get_arg_val<uint32_t>(4);

    // L1 memory layout
    constexpr uint32_t L1_GAME_MEMORY = 0x10000;  // 256KB for game
    constexpr uint32_t L1_OUTPUT = 0x50000;       // Output buffer
    constexpr uint32_t GAME_READ_SIZE = 65536;    // Read 64KB of game
    constexpr uint32_t OUTPUT_SIZE = 4096;

    // Read game data into L1
    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = game_data_dram,
        .page_size = GAME_READ_SIZE
    };
    uint64_t game_noc = get_noc_addr(0, game_gen);
    noc_async_read(game_noc, L1_GAME_MEMORY, GAME_READ_SIZE);
    noc_async_read_barrier();

    // Initialize Z-machine state
    ZMachineState zm;
    zm.memory = (uint8_t*)L1_GAME_MEMORY;
    zm.pc = 0x50D5;  // Initial PC from header
    zm.stack_ptr = 0;
    zm.output_buffer = (char*)L1_OUTPUT;
    zm.output_pos = 0;

    // Write header
    const char* header = "=== EXECUTING ZORK ON BLACKHOLE! ===\n\n";
    while (*header) {
        zm.output_buffer[zm.output_pos++] = *header++;
    }

    // Execute instructions (max 200 to find PRINT instructions)
    for (uint32_t i = 0; i < 200; i++) {
        uint32_t result = execute_instruction(&zm);
        if (result != 0) {
            const char* msg = "\n\n[Execution stopped after instruction ";
            while (*msg) {
                zm.output_buffer[zm.output_pos++] = *msg++;
            }
            // Write instruction count
            if (i >= 100) {
                zm.output_buffer[zm.output_pos++] = '0' + (i / 100);
                zm.output_buffer[zm.output_pos++] = '0' + ((i % 100) / 10);
                zm.output_buffer[zm.output_pos++] = '0' + (i % 10);
            } else if (i >= 10) {
                zm.output_buffer[zm.output_pos++] = '0' + (i / 10);
                zm.output_buffer[zm.output_pos++] = '0' + (i % 10);
            } else {
                zm.output_buffer[zm.output_pos++] = '0' + i;
            }
            zm.output_buffer[zm.output_pos++] = ']';
            zm.output_buffer[zm.output_pos++] = '\n';
            break;
        }
    }

    // Footer with future architecture note
    const char* footer = "\n--- FUTURE: I/O will use HTTP to vLLM on other chips! ---\n";
    while (*footer) {
        zm.output_buffer[zm.output_pos++] = *footer++;
    }
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
