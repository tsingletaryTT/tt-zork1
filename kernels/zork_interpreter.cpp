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
static zword global_vars_addr;  // Address of global variables table

// Call frame for routine calls
struct Frame {
    zbyte* ret_pc;       // Where to return to
    zbyte num_locals;    // How many local variables this routine has
    zword locals[15];    // The local variable values
    zbyte store_var;     // Where to store the return value
};
static Frame frames[64];
static uint32_t frame_sp;

// Flag to stop execution
static bool finished;

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
 * Read a variable value (like Ruby reading a variable)
 *
 * Variable numbering:
 * 0x00 = pop from stack (like Ruby's array.pop)
 * 0x01-0x0F = local variables (like Ruby's local vars in a method)
 * 0x10-0xFF = global variables (like Ruby's $global_vars)
 */
static zword read_variable(zbyte var) {
    if (var == 0) {
        // Pop from stack - like Ruby's: value = stack.pop
        if (sp > 0) {
            return stack[--sp];
        }
        return 0;
    } else if (var >= 0x01 && var <= 0x0F) {
        // Local variable - like Ruby accessing a method's local variable
        if (frame_sp > 0) {
            zbyte local_num = var - 1;
            Frame& current_frame = frames[frame_sp - 1];
            if (local_num < current_frame.num_locals) {
                return current_frame.locals[local_num];
            }
        }
        return 0;
    } else {
        // Global variable - like Ruby's $my_global_variable
        // Globals are stored at global_vars_addr in memory
        zword addr = global_vars_addr + ((var - 0x10) * 2);
        return read_word(addr);
    }
}

/**
 * Write a variable value (like Ruby assigning to a variable)
 */
static void write_variable(zbyte var, zword value) {
    if (var == 0) {
        // Push to stack - like Ruby's: stack.push(value)
        if (sp < 1024) {
            stack[sp++] = value;
        }
    } else if (var >= 0x01 && var <= 0x0F) {
        // Local variable - like Ruby: local_var = value
        if (frame_sp > 0) {
            zbyte local_num = var - 1;
            Frame& current_frame = frames[frame_sp - 1];
            if (local_num < current_frame.num_locals) {
                current_frame.locals[local_num] = value;
            }
        }
    } else {
        // Global variable - like Ruby: $my_global = value
        zword addr = global_vars_addr + ((var - 0x10) * 2);
        write_word(addr, value);
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
        zargs[zargc] = read_variable(var);  // NOW IT WORKS!
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
 * CALL opcode - Call a routine (like Ruby calling a method!)
 *
 * In Ruby:  result = my_function(arg1, arg2)
 * In Z-machine: CALL routine_addr arg1 arg2 -> store_var
 */
static void op_call() {
    // zargs[0] = routine address (like the function name in Ruby)
    // zargs[1..n] = arguments (like the parameters in Ruby)

    zword routine_addr = zargs[0];

    // Special case: calling address 0 means "do nothing, return false"
    if (routine_addr == 0) {
        zbyte store_var;
        CODE_BYTE(store_var);
        write_variable(store_var, 0);  // Return 0 (false)
        return;
    }

    // 1. Read where to store the return value
    zbyte store_var;
    CODE_BYTE(store_var);

    // 2. SAVE OUR CURRENT STATE (like Ruby remembering where we are)
    Frame new_frame;
    new_frame.ret_pc = pc;  // "Come back to this address when done"
    new_frame.store_var = store_var;  // "Store result in this variable"

    // 3. Go to the routine and read its header
    // Multiply by 2 because addresses in v3 are word-addresses
    uint32_t byte_addr = routine_addr * 2;
    if (byte_addr >= 86000) return;  // Safety check

    zbyte* routine_ptr = memory + byte_addr;
    zbyte num_locals = *routine_ptr++;  // "This function has N local variables"
    new_frame.num_locals = num_locals;

    // 4. Initialize local variables (like Ruby creating local vars in a method)
    for (int i = 0; i < num_locals; i++) {
        if ((uint32_t)i + 1 < zargc) {
            // We passed this as an argument (i+1 because arg 0 is the routine address)
            new_frame.locals[i] = zargs[i + 1];
        } else {
            // Not passed - read default value from routine header
            zword default_val = (routine_ptr[0] << 8) | routine_ptr[1];
            routine_ptr += 2;
            new_frame.locals[i] = default_val;
        }
    }

    // 5. PUSH this frame onto call stack (like Ruby adding to stack trace)
    if (frame_sp < 64) {
        frames[frame_sp++] = new_frame;
    }

    // 6. JUMP to the routine! (like Ruby jumping to the method definition)
    pc = routine_ptr;
}

/**
 * RET opcode - Return from routine with a value
 *
 * In Ruby:  return value
 * In Z-machine: RET value
 */
static void op_ret() {
    if (frame_sp == 0) {
        finished = true;  // No frames left, game is done!
        return;
    }

    // 1. Get the return value
    zword return_value = zargs[0];

    // 2. POP the call stack (like Ruby finishing a method)
    frame_sp--;
    Frame frame = frames[frame_sp];

    // 3. GO BACK to where we came from (like Ruby jumping back to caller)
    pc = frame.ret_pc;

    // 4. Store the return value where requested
    write_variable(frame.store_var, return_value);
}

/**
 * RTRUE opcode - return TRUE
 *
 * In Ruby: return true
 * In Z-machine: RTRUE
 */
static void op_rtrue() {
    if (frame_sp == 0) {
        finished = true;
        return;
    }

    // Return value of 1 (TRUE)
    frame_sp--;
    Frame frame = frames[frame_sp];
    pc = frame.ret_pc;
    write_variable(frame.store_var, 1);  // TRUE = 1
}

/**
 * RFALSE opcode - return FALSE
 *
 * In Ruby: return false
 * In Z-machine: RFALSE
 */
static void op_rfalse() {
    if (frame_sp == 0) {
        finished = true;
        return;
    }

    // Return value of 0 (FALSE)
    frame_sp--;
    Frame frame = frames[frame_sp];
    pc = frame.ret_pc;
    write_variable(frame.store_var, 0);  // FALSE = 0
}

/**
 * Main interpreter loop - based on Frotz's interpret()
 *
 * This is like Ruby's "eval" - it reads bytecode and executes it!
 */
static void interpret(uint32_t max_instructions) {
    uint32_t instructions = 0;
    finished = false;

    while (!finished && instructions < max_instructions && (uint32_t)(pc - memory) < 86000) {
        zbyte opcode;
        CODE_BYTE(opcode);
        zargc = 0;

        instructions++;

        // Decode opcode format (from Frotz process.c lines 272-294)
        if (opcode < 0x80) {
            // 2OP opcodes (long form) - like Ruby: a + b, a == b, etc.
            load_operand((opcode & 0x40) ? 2 : 1);
            load_operand((opcode & 0x20) ? 2 : 1);

            zbyte op_num = opcode & 0x1f;
            // For now, just recognize a few important ones
            // (Full Frotz has ~30 2OP opcodes)

        } else if (opcode < 0xb0) {
            // 1OP opcodes (short form) - like Ruby: -a, !a, call(a)
            load_operand(opcode >> 4);

            zbyte op_num = opcode & 0x0f;
            switch (op_num) {
                case 0x0B:  // RET - like Ruby: return value
                    op_ret();
                    break;
                // Other 1OP opcodes we'll add as needed
            }

        } else if (opcode < 0xc0) {
            // 0OP opcodes (no operands) - like Ruby: return, break, next
            zbyte op_num = opcode - 0xb0;
            switch (op_num) {
                case 0:  // RTRUE - like Ruby: return true
                    op_rtrue();
                    break;
                case 1:  // RFALSE - like Ruby: return false
                    op_rfalse();
                    break;
                case 2:  // PRINT - like Ruby: puts "text"
                    op_print();
                    break;
                case 3:  // PRINT_RET - like Ruby: puts "text"; return true
                    op_print_ret();
                    break;
                case 11: // NEW_LINE - like Ruby: puts
                    op_new_line();
                    break;
                default:
                    // Unknown opcode - skip
                    break;
            }
        } else {
            // VAR opcodes (variable number of operands)
            zbyte specifier1;
            CODE_BYTE(specifier1);
            load_all_operands(specifier1);

            zbyte op_num = opcode - 0xc0;
            switch (op_num) {
                case 0:  // CALL - like Ruby: result = my_function(args...)
                    op_call();
                    break;
                // Many more VAR opcodes exist (STOREW, LOADW, etc.)
                // We'll add them as needed
            }
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
    abbrev_table = read_word(0x18);      // Abbreviations table
    global_vars_addr = read_word(0x0C);  // Global variables table
    zword initial_pc = read_word(0x06);  // Initial program counter
    pc = memory + initial_pc;
    sp = 0;         // Stack is empty
    frame_sp = 0;   // No call frames yet
    finished = false;

    const char* h = "╔════════════════════════════════════════════════════╗\n";
    while (*h) output[out_pos++] = *h++;
    h = "║  Z-MACHINE INTERPRETER WITH CALL/RET!            ║\n";
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

    h = "Globals: 0x";
    while (*h) output[out_pos++] = *h++;
    output[out_pos++] = hex[(global_vars_addr >> 12) & 0xF];
    output[out_pos++] = hex[(global_vars_addr >> 8) & 0xF];
    output[out_pos++] = hex[(global_vars_addr >> 4) & 0xF];
    output[out_pos++] = hex[global_vars_addr & 0xF];
    output[out_pos++] = '\n';
    output[out_pos++] = '\n';

    h = "=== EXECUTING Z-MACHINE CODE ===\n\n";
    while (*h) output[out_pos++] = *h++;

    // Run the interpreter! More instructions now that we have CALL/RET
    interpret(1000);  // Execute up to 1000 instructions

    output[out_pos++] = '\n';
    h = "\n=== EXECUTION COMPLETE ===\n";
    while (*h) output[out_pos++] = *h++;

    if (finished) {
        h = "(Game returned from main routine)\n";
        while (*h) output[out_pos++] = *h++;
    }

    output[out_pos++] = '\0';

    // Write output
    InterleavedAddrGen<true> out_gen = {.bank_base_address = out_dram, .page_size = 4096};
    noc_async_write(L1_OUT, get_noc_addr(0, out_gen), 16384);
    noc_async_write_barrier();
}
