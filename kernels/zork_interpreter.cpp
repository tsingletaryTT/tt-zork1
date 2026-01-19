// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_interpreter.cpp - Minimal Z-machine interpreter based on Frotz!
 *
 * This implements the core Frotz interpret() loop and essential opcodes
 * in a single self-contained RISC-V kernel.
 *
 * Based on Frotz's process.c interpret() function, adapted for RISC-V.
 */

#include <cstdint>

typedef uint8_t zbyte;
typedef uint16_t zword;

// Z-machine state
static zbyte* memory;           // Game memory
static zbyte* pc;               // Program counter (pointer into memory)
static zword stack[1024];       // Z-machine stack
static uint32_t sp;             // Stack pointer
static zword zargs[8];          // Current instruction arguments
static uint32_t zargc;          // Number of arguments
static char* output;            // Output buffer
static uint32_t out_pos;
static zword abbrev_table;

// Call frame for routine calls
struct Frame {
    zbyte* ret_pc;
    zbyte num_locals;
    zword locals[15];
    zword store_var;
};
static Frame frames[64];
static uint32_t frame_sp;

// Read byte and advance PC
#define CODE_BYTE(v) v = *pc++

// Read word (big-endian) and advance PC
#define CODE_WORD(v) v = (*pc << 8) | *(pc+1); pc += 2

/**
 * Read byte from memory address
 */
static inline zbyte read_byte(uint32_t addr) {
    if (addr >= 86000) return 0;
    return memory[addr];
}

/**
 * Read word from memory address
 */
static inline zword read_word(uint32_t addr) {
    if (addr >= 86000) return 0;
    return (memory[addr] << 8) | memory[addr + 1];
}

/**
 * Write word to memory
 */
static inline void write_word(uint32_t addr, zword value) {
    if (addr < 86000) {
        memory[addr] = (value >> 8) & 0xFF;
        memory[addr + 1] = value & 0xFF;
    }
}

/**
 * Get character from alphabet
 */
static char get_char(int alph, int idx) {
    if (alph == 0) {
        if (idx == 0) return ' ';
        if (idx >= 6 && idx <= 31) return 'a' + (idx - 6);
    } else if (alph == 1) {
        if (idx == 0) return ' ';
        if (idx >= 6 && idx <= 31) return 'A' + (idx - 6);
    } else {
        const char* punct = " ^0123456789.,!?_#'\"/\\-:()";
        if (idx < 26) return punct[idx];
    }
    return '?';
}

// Forward declaration
static void decode_zstring(uint32_t addr, uint32_t max_words, uint32_t depth);

/**
 * Decode abbreviation
 */
static void decode_abbrev(zbyte code, zbyte index, uint32_t depth) {
    if (depth >= 3 || code < 1 || code > 3) return;

    uint32_t idx = ((code - 1) * 32) + index;
    uint32_t entry_addr = abbrev_table + (idx * 2);
    if (entry_addr >= 86000) return;

    zword word_addr = read_word(entry_addr);
    uint32_t byte_addr = word_addr * 2;
    if (byte_addr < 86000) {
        decode_zstring(byte_addr, 30, depth + 1);
    }
}

/**
 * Decode Z-string with abbreviations
 */
static void decode_zstring(uint32_t addr, uint32_t max_words, uint32_t depth) {
    if (addr >= 86000 || depth >= 3) return;

    int shift = 0;
    zbyte abbrev = 0;

    for (uint32_t w = 0; w < max_words && addr < 86000; w++) {
        zword word = read_word(addr);
        addr += 2;

        for (int s = 10; s >= 0; s -= 5) {
            zbyte c = (word >> s) & 0x1F;

            if (abbrev) {
                decode_abbrev(abbrev, c, depth);
                abbrev = 0;
                shift = 0;
                continue;
            }

            if (c >= 6) {
                if (out_pos < 15000) output[out_pos++] = get_char(shift, c);
                shift = 0;
            } else if (c == 0) {
                if (out_pos < 15000) output[out_pos++] = ' ';
                shift = 0;
            } else if (c >= 1 && c <= 3) {
                abbrev = c;
            } else if (c == 4 || c == 5) {
                shift = (c == 4) ? 1 : 2;
            }
        }

        if (word & 0x8000) break;
    }
}

/**
 * Load operand based on type
 * Type 0 = large constant (word follows)
 * Type 1 = small constant (byte follows)
 * Type 2 = variable
 */
static void load_operand(int type) {
    if (zargc >= 8) return;

    if (type == 0) {
        // Large constant - read word
        CODE_WORD(zargs[zargc]);
    } else if (type == 1) {
        // Small constant - read byte
        zbyte b;
        CODE_BYTE(b);
        zargs[zargc] = b;
    } else {
        // Variable - read variable number then get value
        zbyte var;
        CODE_BYTE(var);
        // TODO: Implement read_variable()
        zargs[zargc] = var;  // Placeholder
    }
    zargc++;
}

/**
 * Load all operands based on specifier byte
 * Bits 7-6: type of arg1 (00=large, 01=small, 10=var, 11=omitted)
 * Bits 5-4: type of arg2
 * Bits 3-2: type of arg3
 * Bits 1-0: type of arg4
 */
static void load_all_operands(zbyte specifier) {
    for (int i = 6; i >= 0; i -= 2) {
        int type = (specifier >> i) & 0x03;
        if (type == 3) break;  // 11 = no more operands
        load_operand(type);
    }
}

/**
 * PRINT opcode - print literal Z-string
 */
static void op_print() {
    uint32_t addr = pc - memory;  // Current PC as address
    decode_zstring(addr, 30, 0);

    // Skip over the string
    while ((uint32_t)(pc - memory) < 86000) {
        zword word = (pc[0] << 8) | pc[1];
        pc += 2;
        if (word & 0x8000) break;  // End bit
    }
}

/**
 * PRINT_RET opcode - print and return TRUE
 */
static void op_print_ret() {
    op_print();
    if (out_pos < 15000) output[out_pos++] = '\n';
    // TODO: Implement return
}

/**
 * NEW_LINE opcode
 */
static void op_new_line() {
    if (out_pos < 15000) output[out_pos++] = '\n';
}

/**
 * RTRUE opcode - return TRUE
 */
static void op_rtrue() {
    // TODO: Implement return
}

/**
 * Main interpreter loop - based on Frotz's interpret()
 */
static void interpret(uint32_t max_instructions) {
    uint32_t instructions = 0;

    while (instructions < max_instructions && (uint32_t)(pc - memory) < 86000) {
        zbyte opcode;
        CODE_BYTE(opcode);
        zargc = 0;

        instructions++;

        // Decode opcode format (from Frotz process.c lines 272-294)
        if (opcode < 0x80) {
            // 2OP opcodes (long form)
            load_operand((opcode & 0x40) ? 2 : 1);
            load_operand((opcode & 0x20) ? 2 : 1);
            // TODO: Call appropriate 2OP handler
        } else if (opcode < 0xb0) {
            // 1OP opcodes (short form)
            load_operand(opcode >> 4);
            // TODO: Call appropriate 1OP handler
        } else if (opcode < 0xc0) {
            // 0OP opcodes
            zbyte op_num = opcode - 0xb0;
            switch (op_num) {
                case 2:  // PRINT
                    op_print();
                    break;
                case 3:  // PRINT_RET
                    op_print_ret();
                    break;
                case 11: // NEW_LINE
                    op_new_line();
                    break;
                case 0:  // RTRUE
                    op_rtrue();
                    break;
                default:
                    // Unknown opcode - skip
                    break;
            }
        } else {
            // VAR opcodes
            zbyte specifier1;
            CODE_BYTE(specifier1);
            load_all_operands(specifier1);
            // TODO: Call appropriate VAR handler
        }
    }
}

/**
 * Kernel main
 */
void kernel_main() {
    uint32_t game_dram = get_arg_val<uint32_t>(0);
    uint32_t out_dram = get_arg_val<uint32_t>(4);

    constexpr uint32_t L1_GAME = 0x10000;
    constexpr uint32_t L1_OUT = 0x50000;

    // Load game data
    InterleavedAddrGen<true> gen = {.bank_base_address = game_dram, .page_size = 1024};
    for (uint32_t off = 0; off < 86838; off += 1024) {
        uint32_t sz = (off + 1024 <= 86838) ? 1024 : (86838 - off);
        noc_async_read(get_noc_addr(off / 1024, gen), L1_GAME + off, sz);
    }
    noc_async_read_barrier();

    memory = (zbyte*)L1_GAME;
    output = (char*)L1_OUT;
    out_pos = 0;

    // Initialize Z-machine state
    abbrev_table = read_word(0x18);
    zword initial_pc = read_word(0x06);
    pc = memory + initial_pc;
    sp = 0;
    frame_sp = 0;

    const char* h = "╔════════════════════════════════════════════════════╗\n";
    while (*h) output[out_pos++] = *h++;
    h = "║  Z-MACHINE INTERPRETER (based on Frotz!)         ║\n";
    while (*h) output[out_pos++] = *h++;
    h = "╚════════════════════════════════════════════════════╝\n\n";
    while (*h) output[out_pos++] = *h++;

    h = "Initial PC: 0x";
    while (*h) output[out_pos++] = *h++;
    const char* hex = "0123456789ABCDEF";
    output[out_pos++] = hex[(initial_pc >> 12) & 0xF];
    output[out_pos++] = hex[(initial_pc >> 8) & 0xF];
    output[out_pos++] = hex[(initial_pc >> 4) & 0xF];
    output[out_pos++] = hex[initial_pc & 0xF];
    output[out_pos++] = '\n';
    output[out_pos++] = '\n';

    h = "=== EXECUTING Z-MACHINE CODE ===\n\n";
    while (*h) output[out_pos++] = *h++;

    // Run the interpreter!
    interpret(200);  // Execute up to 200 instructions

    output[out_pos++] = '\n';
    h = "\n=== EXECUTION COMPLETE ===\n";
    while (*h) output[out_pos++] = *h++;
    output[out_pos++] = '\0';

    // Write output
    InterleavedAddrGen<true> out_gen = {.bank_base_address = out_dram, .page_size = 4096};
    noc_async_write(L1_OUT, get_noc_addr(0, out_gen), 16384);
    noc_async_write_barrier();
}
