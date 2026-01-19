// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_minimal_executor.cpp - Minimal Z-machine opcode executor!
 *
 * Goal: Execute enough Z-machine opcodes to run the game's initialization
 * and display the opening text.
 *
 * Architecture:
 * - PC (Program Counter): Current execution address
 * - Stack: For routine calls and local variables
 * - Variables: Global variables + local variables
 * - Output: Text output buffer
 *
 * Essential Opcodes to Implement:
 * - PRINT (0xB2): Print literal string
 * - PRINT_RET (0xB3): Print and return TRUE
 * - CALL: Call a routine (various forms)
 * - RET/RTRUE/RFALSE: Return from routine
 * - STORE: Store value to variable
 * - JUMP: Unconditional jump
 * - JZ: Jump if zero
 * - And a few more for basic execution
 */

#include <cstdint>

typedef uint8_t zbyte;
typedef uint16_t zword;

// Z-machine state
static zbyte* memory;           // Game memory
static char* output;            // Output buffer
static uint32_t output_pos;
static zword pc;                // Program counter
static zword stack[256];        // Stack for calls and variables
static uint32_t sp;             // Stack pointer
static zword abbrev_table;      // Abbreviation table address
static zword global_var_base;   // Global variables base address

// Call stack for routine calls
struct CallFrame {
    zword return_pc;            // Where to return to
    zbyte num_locals;           // Number of local variables
    zword locals[15];           // Local variable values
};
static CallFrame call_stack[32];
static uint32_t call_depth;

/**
 * Read byte from memory
 */
static inline zbyte read_byte(zword addr) {
    if (addr >= 86000) return 0;
    return memory[addr];
}

/**
 * Read word (16-bit big-endian) from memory
 */
static inline zword read_word(zword addr) {
    if (addr >= 86000) return 0;
    return (memory[addr] << 8) | memory[addr + 1];
}

/**
 * Write byte to memory
 */
static inline void write_byte(zword addr, zbyte value) {
    if (addr < 86000) memory[addr] = value;
}

/**
 * Write word to memory
 */
static inline void write_word(zword addr, zword value) {
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

/**
 * Decode abbreviation
 */
static void decode_abbrev(zbyte code, zbyte index, uint32_t depth);

/**
 * Decode Z-string with abbreviations
 */
static void decode_zstring(zword addr, uint32_t max_words, uint32_t depth) {
    if (addr >= 86000 || depth >= 5) return;

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
                if (output_pos < 15000) output[output_pos++] = get_char(shift, c);
                shift = 0;
            } else if (c == 0) {
                if (output_pos < 15000) output[output_pos++] = ' ';
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
 * Decode abbreviation
 */
static void decode_abbrev(zbyte code, zbyte index, uint32_t depth) {
    if (depth >= 5 || code < 1 || code > 3 || index > 31) return;

    uint32_t idx = ((code - 1) * 32) + index;
    zword entry = abbrev_table + (idx * 2);
    if (entry >= 86000) return;

    zword word_addr = read_word(entry);
    zword byte_addr = word_addr * 2;
    if (byte_addr < 86000) {
        decode_zstring(byte_addr, 30, depth + 1);
    }
}

/**
 * Read variable value
 * 0x00 = pop from stack
 * 0x01-0x0F = local variables
 * 0x10-0xFF = global variables
 */
static zword read_variable(zbyte var) {
    if (var == 0) {
        // Pop from stack
        if (sp > 0) return stack[--sp];
        return 0;
    } else if (var <= 0x0F) {
        // Local variable
        if (call_depth > 0) {
            zbyte local_idx = var - 1;
            if (local_idx < call_stack[call_depth - 1].num_locals) {
                return call_stack[call_depth - 1].locals[local_idx];
            }
        }
        return 0;
    } else {
        // Global variable
        zword addr = global_var_base + ((var - 0x10) * 2);
        return read_word(addr);
    }
}

/**
 * Write variable value
 */
static void write_variable(zbyte var, zword value) {
    if (var == 0) {
        // Push to stack
        if (sp < 256) stack[sp++] = value;
    } else if (var <= 0x0F) {
        // Local variable
        if (call_depth > 0) {
            zbyte local_idx = var - 1;
            if (local_idx < call_stack[call_depth - 1].num_locals) {
                call_stack[call_depth - 1].locals[local_idx] = value;
            }
        }
    } else {
        // Global variable
        zword addr = global_var_base + ((var - 0x10) * 2);
        write_word(addr, value);
    }
}

/**
 * Execute PRINT opcode - print literal string following PC
 */
static void op_print() {
    decode_zstring(pc, 30, 0);

    // Skip over the string
    while (pc < 86000) {
        zword word = read_word(pc);
        pc += 2;
        if (word & 0x8000) break;  // End bit
    }
}

/**
 * Execute PRINT_RET opcode - print and return TRUE
 */
static void op_print_ret() {
    op_print();
    if (output_pos < 15000) output[output_pos++] = '\n';

    // Return TRUE (1) from current routine
    // In a full implementation, this would actually return
    // For now, just note it
}

/**
 * Newline
 */
static void op_new_line() {
    if (output_pos < 15000) output[output_pos++] = '\n';
}

/**
 * Simplified executor - just scan for PRINT opcodes and execute them
 */
static void execute_from_pc(uint32_t max_instructions) {
    for (uint32_t i = 0; i < max_instructions && pc < 86000; i++) {
        zbyte opcode = read_byte(pc);
        pc++;

        // Check opcode type
        if (opcode == 0xB2) {
            // PRINT
            op_print();
        } else if (opcode == 0xB3) {
            // PRINT_RET
            op_print_ret();
        } else if (opcode == 0xBB) {
            // NEW_LINE
            op_new_line();
        } else {
            // For now, skip unknown opcodes
            // A full implementation would decode all opcode forms
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
    output_pos = 0;

    // Initialize Z-machine state
    abbrev_table = read_word(0x18);
    global_var_base = read_word(0x0C);
    pc = read_word(0x06);  // Initial PC from header
    sp = 0;
    call_depth = 0;

    const char* h = "╔════════════════════════════════════════════════════╗\n";
    while (*h) output[output_pos++] = *h++;
    h = "║  Z-MACHINE EXECUTOR ON BLACKHOLE RISC-V!         ║\n";
    while (*h) output[output_pos++] = *h++;
    h = "╚════════════════════════════════════════════════════╝\n\n";
    while (*h) output[output_pos++] = *h++;

    h = "Initial PC: 0x";
    while (*h) output[output_pos++] = *h++;
    const char* hex = "0123456789ABCDEF";
    output[output_pos++] = hex[(pc >> 12) & 0xF];
    output[output_pos++] = hex[(pc >> 8) & 0xF];
    output[output_pos++] = hex[(pc >> 4) & 0xF];
    output[output_pos++] = hex[pc & 0xF];
    output[output_pos++] = '\n';
    output[output_pos++] = '\n';

    h = "=== EXECUTING Z-MACHINE CODE ===\n\n";
    while (*h) output[output_pos++] = *h++;

    // Execute instructions
    execute_from_pc(100);  // Try first 100 instructions

    output[output_pos++] = '\n';
    h = "\n=== EXECUTION COMPLETE ===\n";
    while (*h) output[output_pos++] = *h++;
    output[output_pos++] = '\0';

    // Write output
    InterleavedAddrGen<true> out_gen = {.bank_base_address = out_dram, .page_size = 4096};
    noc_async_write(L1_OUT, get_noc_addr(0, out_gen), 16384);
    noc_async_write_barrier();
}
