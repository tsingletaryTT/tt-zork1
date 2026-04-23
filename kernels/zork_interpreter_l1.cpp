// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_interpreter_l1.cpp — Z-machine interpreter with L1-resident large arrays.
 *
 * This is a derived version of zork_interpreter_opt.cpp adapted for the newer
 * TT-Metal / TT-Lang Python environment (QB2, firmware build 6745986192171285359).
 *
 * Problem: The newer firmware leaves only ~4.8 KB of thread-local SRAM for kernel
 * static data (.bss + .data), but the interpreter's large static arrays
 * (stack[1024] = 2048 B, frames[64] = 2304 B) total ~5.8 KB — overflowing the limit.
 *
 * Fix: Replace the large static arrays with pointers initialised in kernel_main()
 * to point into spare L1 SRAM between the game-data region (ends at 0x25400) and
 * the output buffer (starts at 0x30000). The 43 KB gap is more than enough.
 *
 * L1 layout (unchanged from opt version):
 *   0x10000  L1_GAME   — 87040 B game data  (ends at 0x25400)
 *   0x26000  L1_STACK  — 2048 B  Z-machine stack  (stack[1024] zword)
 *   0x26800  L1_FRAMES — 2400 B  call frames  (frames[64])
 *   0x27200  L1_OPCODES— 64 B    opcode trace buffer
 *   0x30000  L1_OUT    — 16384 B output text
 *   0x34000  L1_INPUT  — 1024 B  input command
 *   0x50000  L1_STATE  — state snapshot (only when STATE_DRAM_ADDR defined)
 *
 * Everything else is identical to zork_interpreter_opt.cpp.
 * DO NOT add new large static arrays to this file.
 *
 * Based on Frotz's process.c interpret() function, adapted for RISC-V.
 */

#include <cstdint>
#include "api/dataflow/dataflow_api.h"

// DRAM addresses passed via compile-time defines from host
#ifndef GAME_DRAM_ADDR
#error "GAME_DRAM_ADDR must be defined"
#endif
#ifndef OUTPUT_DRAM_ADDR
#error "OUTPUT_DRAM_ADDR must be defined"
#endif
#ifndef INPUT_DRAM_ADDR
#error "INPUT_DRAM_ADDR must be defined"
#endif

typedef uint8_t zbyte;
typedef uint16_t zword;

// Z-machine state
static zbyte* memory;           // Game memory
static zbyte* pc;               // Program counter (pointer into memory)
// NOTE: stack, frames, and first_opcodes are declared as POINTERS (not arrays)
// and initialised to L1 SRAM addresses in kernel_main().  This keeps the .bss
// section small enough to fit the ~4.8 KB thread-local region limit.
static zword* stack;            // Z-machine stack — points into L1_STACK (0x26000)
static uint32_t sp;             // Stack pointer
static zword zargs[8];          // Current instruction arguments (small: 16 bytes)
static uint32_t zargc;          // Number of arguments
static char* output;            // Output buffer
static uint32_t out_pos;
static char* input;             // Input buffer (from host)
static zword abbrev_table;
static zword global_vars_addr;  // Address of global variables table
static zword dictionary_addr;   // Address of dictionary table

// Call frame for routine calls
struct Frame {
    zbyte* ret_pc;       // Where to return to
    zbyte num_locals;    // How many local variables this routine has
    zword locals[15];    // The local variable values
    zbyte store_var;     // Where to store the return value
};
// NOTE: frames pointer is initialised in kernel_main() to L1_FRAMES (0x26800)
static Frame* frames;           // Call frame stack — points into L1_FRAMES (0x26800)
static uint32_t frame_sp;

// Flag to stop execution
static bool finished;

// Opcode tracking — pointer initialised to L1_OPCODES (0x27200) in kernel_main()
static zbyte* first_opcodes;    // Just the raw opcodes, not counts
static uint32_t opcode_track_count;

/**
 * Z-machine state snapshot for persistence between kernel invocations
 * This allows us to run interpret() in batches of 100 instructions
 */
struct ZMachineState {
    uint32_t pc_offset;          // PC as offset from memory base (not pointer!)
    uint32_t sp;                 // Stack pointer
    zword stack[1024];           // Stack contents
    uint32_t frame_sp;           // Call frame stack pointer
    Frame frames[64];            // Call frames
    bool finished;               // Execution finished flag
    uint32_t out_pos;            // Output buffer position
    uint32_t instruction_count;  // Total instructions executed across all batches
};

// Debug counters
static uint32_t print_obj_calls = 0;

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
 * idx is the raw 5-bit character code (0-31)
 * - Codes 0-5 are special (handled elsewhere)
 * - Codes 6-31 map to alphabet positions 0-25
 *
 * Alphabet 2 positions (Z-machine spec):
 * Position 0 (code 6): space
 * Position 1 (code 7): newline
 * Position 2+ (code 8+): 0123456789.,!?_#'"/\-:()
 */
static char get_char(int alph, int idx) {
    if (alph == 0) {
        if (idx == 0) return ' ';
        if (idx >= 6 && idx <= 31) return 'a' + (idx - 6);
    } else if (alph == 1) {
        if (idx == 0) return ' ';
        if (idx >= 6 && idx <= 31) return 'A' + (idx - 6);
    } else {
        // Alphabet 2: punctuation and special characters
        if (idx == 0) return ' ';
        if (idx == 6) return ' ';   // Position 0
        if (idx == 7) return '\n';  // Position 1 - NEWLINE!
        // Positions 2-25: 0123456789.,!?_#'"/\-:()
        const char* punct = "0123456789.,!?_#'\"/\\-:()";
        if (idx >= 8 && idx <= 31) return punct[idx - 8];
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
 * Convert nibble (0-15) to hex character
 */
static char tohex(uint8_t nibble) {
    return (nibble < 10) ? ('0' + nibble) : ('A' + (nibble - 10));
}

/**
 * Load operand based on type (matches Frotz's implementation)
 * Uses bit tests for robustness (from Frotz process.c lines 197-216)
 *
 * Bit 1 set (type & 2): variable
 * Bit 0 set (type & 1): small constant
 * Neither bit set: large constant
 */
static void load_operand(int type) {
    if (zargc >= 8) return;

    if (type & 2) {
        // Variable - bit 1 set
        zbyte var;
        CODE_BYTE(var);
        zargs[zargc] = read_variable(var);
    } else if (type & 1) {
        // Small constant - bit 0 set
        zbyte b;
        CODE_BYTE(b);
        zargs[zargc] = b;
    } else {
        // Large constant - both bits clear
        CODE_WORD(zargs[zargc]);
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
 * Branch helper - handle conditional branch
 *
 * In Ruby: if condition then goto label end
 * In Z-machine: Branch instructions have offset encoded after them
 */
static void do_branch(bool condition) {
    // Read branch byte
    zbyte branch_byte;
    CODE_BYTE(branch_byte);

    // Bit 7: branch on TRUE (1) or FALSE (0)
    bool branch_on_true = (branch_byte & 0x80) != 0;

    // Bit 6: 0 = branch offset is 14-bit (2 bytes), 1 = 6-bit (in this byte)
    bool short_form = (branch_byte & 0x40) != 0;

    int16_t offset;
    if (short_form) {
        // 6-bit offset in low 6 bits of this byte
        offset = branch_byte & 0x3F;
    } else {
        // 14-bit offset: low 6 bits of first byte + 8 bits of next byte
        zbyte second_byte;
        CODE_BYTE(second_byte);
        offset = ((branch_byte & 0x3F) << 8) | second_byte;

        // Sign extend from 14 bits to 16 bits
        if (offset & 0x2000) {
            offset |= 0xC000;  // Set top 2 bits for negative
        }
    }

    // Should we branch?
    bool do_it = (condition == branch_on_true);

    if (do_it) {
        if (offset == 0 || offset == 1) {
            // Special: return FALSE (0) or TRUE (1)
            if (frame_sp > 0) {
                frame_sp--;
                Frame frame = frames[frame_sp];
                pc = frame.ret_pc;
                write_variable(frame.store_var, offset);
            }
            // If frame_sp <= 0, just ignore (don't set finished)
        } else {
            // Normal branch: offset is relative to current PC
            // Offset of 2 means "skip the next instruction"
            pc += (offset - 2);
        }
    }
}

/**
 * STORE opcode - Store a value to a variable (2OP 0x0D)
 *
 * In Z-machine: STORE variable value
 * zargs[0] = variable number, zargs[1] = value to store
 */
static void op_store() {
    write_variable((zbyte)zargs[0], zargs[1]);
}

/**
 * JL opcode - Jump if Less Than (2OP 0x02)
 */
static void op_jl() {
    do_branch((int16_t)zargs[0] < (int16_t)zargs[1]);
}

/**
 * JG opcode - Jump if Greater Than (2OP 0x03)
 */
static void op_jg() {
    do_branch((int16_t)zargs[0] > (int16_t)zargs[1]);
}

/**
 * INC_CHK opcode - Increment variable and branch if > value (2OP 0x05)
 */
static void op_inc_chk() {
    zbyte var = (zbyte)zargs[0];
    zword v = read_variable(var) + 1;
    write_variable(var, v);
    do_branch((int16_t)v > (int16_t)zargs[1]);
}

/**
 * JIN opcode - Jump if object in parent (2OP 0x06)
 * Stub: always false (no full object tree)
 */
static void op_jin() {
    do_branch(false);
}

/**
 * LOADW opcode - Load word from array (2OP 0x0F)
 *
 * In Z-machine: LOADW array word-index -> (result)
 */
static void op_loadw() {
    zbyte store_var;
    CODE_BYTE(store_var);
    uint32_t addr = (uint32_t)zargs[0] + (uint32_t)zargs[1] * 2;
    write_variable(store_var, (addr + 1 < 87040) ? read_word(addr) : 0);
}

/**
 * LOADB opcode - Load byte from array (2OP 0x10)
 *
 * In Z-machine: LOADB array byte-index -> (result)
 */
static void op_loadb() {
    zbyte store_var;
    CODE_BYTE(store_var);
    uint32_t addr = (uint32_t)zargs[0] + (uint32_t)zargs[1];
    write_variable(store_var, (addr < 87040) ? read_byte(addr) : 0);
}

/**
 * SUB opcode - Subtract (2OP 0x15)
 */
static void op_sub() {
    zbyte store_var;
    CODE_BYTE(store_var);
    write_variable(store_var, (zword)((int16_t)zargs[0] - (int16_t)zargs[1]));
}

/**
 * MUL opcode - Multiply (2OP 0x16)
 */
static void op_mul() {
    zbyte store_var;
    CODE_BYTE(store_var);
    write_variable(store_var, (zword)((int16_t)zargs[0] * (int16_t)zargs[1]));
}

/**
 * DIV opcode - Divide (2OP 0x17)
 */
static void op_div() {
    zbyte store_var;
    CODE_BYTE(store_var);
    if (zargs[1] != 0) {
        write_variable(store_var, (zword)((int16_t)zargs[0] / (int16_t)zargs[1]));
    } else {
        write_variable(store_var, 0);
    }
}

/**
 * MOD opcode - Modulo (2OP 0x18)
 */
static void op_mod() {
    zbyte store_var;
    CODE_BYTE(store_var);
    if (zargs[1] != 0) {
        write_variable(store_var, (zword)((int16_t)zargs[0] % (int16_t)zargs[1]));
    } else {
        write_variable(store_var, 0);
    }
}

/**
 * INC opcode - Increment variable (1OP 0x05)
 */
static void op_inc() {
    zbyte var = (zbyte)zargs[0];
    write_variable(var, read_variable(var) + 1);
}

/**
 * DEC opcode - Decrement variable (1OP 0x06)
 */
static void op_dec() {
    zbyte var = (zbyte)zargs[0];
    write_variable(var, read_variable(var) - 1);
}

/**
 * JUMP opcode - Unconditional jump (1OP 0x0C)
 *
 * Offset is in zargs[0] (signed 16-bit). Destination = pc + offset - 2.
 */
static void op_jump() {
    int16_t offset = (int16_t)zargs[0];
    pc += (offset - 2);
}

/**
 * PRINT_PADDR opcode - Print Z-string at packed address (1OP 0x0D)
 *
 * In V3, packed address × 2 = byte address.
 */
static void op_print_paddr() {
    uint32_t addr = (uint32_t)zargs[0] * 2;
    if (addr < 87040 && out_pos < 14000) {
        decode_zstring(addr, 20, 0);
    }
}

/**
 * NOT opcode - Bitwise NOT (1OP 0x0F)
 */
static void op_not() {
    zbyte store_var;
    CODE_BYTE(store_var);
    write_variable(store_var, ~zargs[0]);
}

/**
 * GET_PROP_LEN opcode - Get property length (1OP 0x04)
 * Stub: returns 0.
 */
static void op_get_prop_len() {
    zbyte store_var;
    CODE_BYTE(store_var);
    write_variable(store_var, 0);
}

/**
 * REMOVE_OBJ opcode - Remove object from tree (1OP 0x09)
 * Stub: no-op.
 */
static void op_remove_obj() {
    // No object tree modification implemented
}

/**
 * LOAD opcode - Load variable value (1OP 0x0E)
 *
 * In Z-machine: LOAD variable -> (result)
 * zargs[0] is the variable number (already decoded from operand).
 * Only the store_var byte remains to be read from the stream.
 */
static void op_load() {
    zbyte store_var;
    CODE_BYTE(store_var);
    write_variable(store_var, read_variable((zbyte)zargs[0]));
}

/**
 * JZ opcode - Jump if Zero
 *
 * In Ruby: goto label if value == 0
 * In Z-machine: JZ value ?branch
 */
static void op_jz() {
    do_branch(zargs[0] == 0);
}

/**
 * JE opcode - Jump if Equal
 *
 * In Ruby: goto label if a == b || a == c || a == d
 * In Z-machine: JE a b c d ?branch
 */
static void op_je() {
    // Compare zargs[0] with zargs[1..zargc-1]
    bool equal = false;
    for (uint32_t i = 1; i < zargc; i++) {
        if (zargs[0] == zargs[i]) {
            equal = true;
            break;
        }
    }
    do_branch(equal);
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
 * PRINT_RET opcode - print inline Z-string, newline, return TRUE (0OP 0x03)
 * Inline RTRUE logic to avoid forward-declaration dependency.
 */
static void op_print_ret() {
    op_print();
    if (out_pos < 15000) output[out_pos++] = '\n';
    if (frame_sp > 0) {
        frame_sp--;
        Frame frame = frames[frame_sp];
        pc = frame.ret_pc;
        write_variable(frame.store_var, 1);
    }
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
    // CRITICAL: ALWAYS advance routine_ptr past the default value, even when an
    // argument overrides it. The routine header has N default words for N locals;
    // all N words are present in the binary regardless of how many args were passed.
    // Skipping routine_ptr+=2 for passed args would leave pc pointing mid-header.
    for (int i = 0; i < num_locals; i++) {
        zword default_val = (routine_ptr[0] << 8) | routine_ptr[1];
        routine_ptr += 2;  // Always consume the default word from the header
        if ((uint32_t)(i + 1) < zargc) {
            // Argument was passed - it overrides the default
            new_frame.locals[i] = zargs[i + 1];
        } else {
            // No argument - use the default from the routine header
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
        // No frames - just ignore the RET and continue
        // (This shouldn't happen in normal gameplay but let's be robust)
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
 * ADD opcode - Add two numbers
 *
 * In Ruby: result = a + b
 * In Z-machine: ADD a b -> result
 */
static void op_add() {
    zbyte store_var;
    CODE_BYTE(store_var);

    // Signed 16-bit addition
    int16_t a = (int16_t)zargs[0];
    int16_t b = (int16_t)zargs[1];
    int16_t result = a + b;

    write_variable(store_var, (zword)result);
}

/**
 * STOREW opcode - Store word in array
 *
 * In Ruby: array[index] = value
 * In Z-machine: STOREW array index value
 */
static void op_storew() {
    // zargs[0] = array base address
    // zargs[1] = word index (not byte index!)
    // zargs[2] = value to store

    uint32_t addr = zargs[0] + (zargs[1] * 2);  // Word index -> byte address
    write_word(addr, zargs[2]);
}

/**
 * GET_SIBLING opcode - Get object's sibling
 *
 * In Z-machine: GET_SIBLING object -> result ?branch
 */
static void op_get_sibling() {
    // For now, just return 0 (no sibling) and don't branch
    zbyte store_var;
    CODE_BYTE(store_var);
    write_variable(store_var, 0);

    // Handle branch (will fail since result is 0)
    do_branch(false);
}

/**
 * PUT_PROP opcode - Write object property
 *
 * In Z-machine: PUT_PROP object property value
 */
static void op_put_prop() {
    // zargs[0] = object number
    // zargs[1] = property number
    // zargs[2] = value to write

    // For now, just ignore property writes (they don't affect opening text)
    // A full implementation would modify the property table in memory
}

/**
 * GET_PROP opcode - Get object property value
 *
 * In Z-machine: GET_PROP object property -> result
 */
static void op_get_prop() {
    zbyte store_var;
    CODE_BYTE(store_var);

    // For now, return 0 (property not found/empty)
    // A full implementation would read from property table
    write_variable(store_var, 0);
}

/**
 * GET_CHILD opcode - Get object's first child
 *
 * In Z-machine: GET_CHILD object -> result ?branch
 */
static void op_get_child() {
    zbyte store_var;
    CODE_BYTE(store_var);

    // Return 0 (no children) and don't branch
    write_variable(store_var, 0);
    do_branch(false);
}

/**
 * GET_PARENT opcode - Get object's parent
 *
 * In Z-machine: GET_PARENT object -> result
 */
static void op_get_parent() {
    zbyte store_var;
    CODE_BYTE(store_var);

    // Return 0 (no parent)
    write_variable(store_var, 0);
}

/**
 * AND opcode - Bitwise AND
 *
 * In Ruby: result = a & b
 * In Z-machine: AND a b -> result
 */
static void op_and() {
    zbyte store_var;
    CODE_BYTE(store_var);

    zword result = zargs[0] & zargs[1];
    write_variable(store_var, result);
}

/**
 * TEST_ATTR opcode - Test if object has attribute
 *
 * In Z-machine: TEST_ATTR object attribute ?branch
 */
static void op_test_attr() {
    // zargs[0] = object number
    // zargs[1] = attribute number (0-31)

    // For now, always return false (attribute not set)
    do_branch(false);
}

/**
 * DEC_CHK opcode - Decrement variable and branch if less than value
 *
 * In Z-machine: DEC_CHK var value ?branch
 */
static void op_dec_chk() {
    // zargs[0] = variable number to decrement
    // zargs[1] = value to compare against

    zbyte var = (zbyte)zargs[0];
    zword current = read_variable(var);

    // Decrement (signed)
    int16_t decremented = (int16_t)current - 1;
    write_variable(var, (zword)decremented);

    // Branch if result < value (signed comparison)
    bool less_than = decremented < (int16_t)zargs[1];
    do_branch(less_than);
}

/**
 * RANDOM opcode - Generate random number
 *
 * In Z-machine: RANDOM range -> result
 * range > 0: returns 1..range
 * range <= 0: seeds RNG with |range|, returns 0
 */
static void op_random() {
    zbyte store_var;
    CODE_BYTE(store_var);

    int16_t range = (int16_t)zargs[0];

    if (range <= 0) {
        // Seed RNG (we ignore seeding for now)
        write_variable(store_var, 0);
    } else {
        // Return pseudo-random number 1..range
        // For now, just return 1 (deterministic but valid)
        write_variable(store_var, 1);
    }
}

/**
 * RTRUE opcode - return TRUE
 *
 * In Ruby: return true
 * In Z-machine: RTRUE
 */
static void op_rtrue() {
    if (frame_sp == 0) {
        // No frames - just ignore
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
        // No frames - just ignore
        return;
    }

    // Return value of 0 (FALSE)
    frame_sp--;
    Frame frame = frames[frame_sp];
    pc = frame.ret_pc;
    write_variable(frame.store_var, 0);  // FALSE = 0
}

/**
 * PRINT_OBJ opcode - Print object name
 *
 * In Z-machine: PRINT_OBJ object_num
 */
static uint8_t last_opcode_for_print_obj = 0;  // Debug: capture triggering opcode

static void op_print_obj() {
    print_obj_calls++;  // Debug counter

    zword obj_num = zargs[0];

    // Only show first call with more detail
    if (print_obj_calls == 1 && out_pos < 14500) {
        output[out_pos++] = '[';
        output[out_pos++] = 'P';
        output[out_pos++] = 'O';
        output[out_pos++] = 'B';
        output[out_pos++] = 'J';
        output[out_pos++] = ' ';
        output[out_pos++] = 'o';
        output[out_pos++] = 'p';
        output[out_pos++] = '=';
        output[out_pos++] = tohex(last_opcode_for_print_obj >> 4);
        output[out_pos++] = tohex(last_opcode_for_print_obj & 0xF);
        output[out_pos++] = ' ';
        output[out_pos++] = 'n';
        output[out_pos++] = '=';
        // Print object number
        if (obj_num >= 100) output[out_pos++] = '0' + (obj_num / 100);
        if (obj_num >= 10) output[out_pos++] = '0' + ((obj_num / 10) % 10);
        output[out_pos++] = '0' + (obj_num % 10);
        output[out_pos++] = ']';
    }

    if (obj_num == 0 || obj_num > 255) return;

    zword obj_table = read_word(0x0A);
    if (obj_table == 0 || obj_table >= 85000) return;

    zword obj_start = obj_table + 62;
    if (obj_start >= 85000) return;

    zword entry = obj_start + ((obj_num - 1) * 9);
    if (entry >= 85000) return;

    zword prop_table = read_word(entry + 7);
    if (prop_table == 0 || prop_table >= 85000) return;

    zbyte text_len = read_byte(prop_table);
    if (text_len == 0 || text_len > 10) return;

    if (prop_table + 1 + (text_len * 2) < 85000) {
        decode_zstring(prop_table + 1, text_len, 0);
    }
}

/**
 * PRINT_ADDR opcode - Print Z-string at address
 *
 * In Z-machine: PRINT_ADDR address
 */
static void op_print_addr() {
    zword addr = zargs[0];
    // Stricter bounds: only decode if in reasonable range
    if (addr > 0 && addr < 85000 && out_pos < 14000) {
        decode_zstring(addr, 10, 0);  // Limit to 10 words max
    }
}

/**
 * PRINT_CHAR opcode - Print a single character
 *
 * In Z-machine: PRINT_CHAR char_code
 */
static void op_print_char() {
    zbyte ch = (zbyte)zargs[0];
    if (out_pos < 15000) output[out_pos++] = ch;
}

/**
 * PRINT_NUM opcode - Print a signed number
 *
 * In Z-machine: PRINT_NUM value
 * This fixes the "Release ixn" bug!
 */
static void op_print_num() {
    int16_t value = (int16_t)zargs[0];

    if (value < 0) {
        if (out_pos < 15000) output[out_pos++] = '-';
        value = -value;
    }

    char digits[6];
    int num_digits = 0;

    if (value == 0) {
        digits[num_digits++] = '0';
    } else {
        while (value > 0 && num_digits < 6) {
            digits[num_digits++] = '0' + (value % 10);
            value /= 10;
        }
    }

    for (int i = num_digits - 1; i >= 0; i--) {
        if (out_pos < 15000) output[out_pos++] = digits[i];
    }
}

/**
 * READ opcode - Read player input from DRAM buffer
 *
 * In Z-machine: READ text_buffer parse_buffer
 *
 * This is the CRITICAL opcode that enables interactive gameplay!
 * Instead of waiting for keyboard input, we read from a DRAM buffer
 * that the host has pre-loaded with the user's command.
 *
 * Arguments:
 *   zargs[0] = text_buffer address (where to store the text the user typed)
 *   zargs[1] = parse_buffer address (where to store parsed word tokens)
 *
 * Text buffer format:
 *   byte 0: max length (how many chars the buffer can hold)
 *   byte 1: actual length (how many chars were typed)
 *   bytes 2+: the actual text characters
 *
 * Parse buffer format:
 *   byte 0: max number of words that can be parsed
 *   byte 1: actual number of words parsed
 *   bytes 2+: word entries (each 4 bytes: dict_addr(2), text_len(1), text_pos(1))
 */
static void op_read() {
    zword text_buffer_addr = zargs[0];
    zword parse_buffer_addr = zargs[1];

    // Read max length from text buffer
    zbyte max_len = read_byte(text_buffer_addr);
    if (max_len == 0) max_len = 80;  // Default if not set

    // Copy input string from L1 input buffer to text buffer in Z-machine memory
    // Input format: null-terminated string
    zbyte actual_len = 0;
    for (uint32_t i = 0; i < max_len && i < 200 && input[i] != '\0'; i++) {
        // Convert to lowercase for Z-machine (standard behavior)
        char ch = input[i];
        if (ch >= 'A' && ch <= 'Z') {
            ch = ch - 'A' + 'a';
        }
        memory[text_buffer_addr + 2 + i] = (zbyte)ch;
        actual_len++;
    }

    // Store actual length in text buffer
    memory[text_buffer_addr + 1] = actual_len;

    // Parse the input into words if parse buffer provided
    if (parse_buffer_addr != 0) {
        // Read max words from parse buffer
        zbyte max_words = read_byte(parse_buffer_addr);
        if (max_words == 0) max_words = 10;  // Default

        zbyte word_count = 0;
        uint32_t pos = 0;

        // Simple tokenization: split by spaces
        while (pos < actual_len && word_count < max_words) {
            // Skip spaces
            while (pos < actual_len && memory[text_buffer_addr + 2 + pos] == ' ') {
                pos++;
            }
            if (pos >= actual_len) break;

            // Found start of word
            uint32_t word_start = pos;
            uint32_t word_len = 0;

            // Read until space or end
            while (pos < actual_len && memory[text_buffer_addr + 2 + pos] != ' ') {
                word_len++;
                pos++;
            }

            // Store word entry in parse buffer
            // Format: dict_addr(2 bytes), text_len(1), text_pos(1)
            uint32_t entry_addr = parse_buffer_addr + 2 + (word_count * 4);

            // For now, set dict_addr to 0 (word not in dictionary)
            // A full implementation would look up each word in the dictionary table
            write_word(entry_addr, 0);           // dict_addr = 0 (not found)
            memory[entry_addr + 2] = word_len;   // length of word
            memory[entry_addr + 3] = word_start + 2;  // position in text buffer

            word_count++;
        }

        // Store actual word count
        memory[parse_buffer_addr + 1] = word_count;
    }

    // Echo the input to output for debugging
    if (out_pos < 14900) {
        const char* prompt = "\n> ";
        while (*prompt && out_pos < 14900) {
            output[out_pos++] = *prompt++;
        }
        for (uint32_t i = 0; i < actual_len && out_pos < 15000; i++) {
            output[out_pos++] = memory[text_buffer_addr + 2 + i];
        }
        if (out_pos < 15000) output[out_pos++] = '\n';
    }
}

/**
 * PUSH opcode - push value onto evaluation stack (VAR 0x08 = 0xE8)
 */
static void op_push() {
    if (sp < 1024) stack[sp++] = zargs[0];
}

/**
 * PULL opcode - pop value from stack into variable (VAR 0x09 = 0xE9)
 */
static void op_pull() {
    zbyte var = (zbyte)zargs[0];
    write_variable(var, (sp > 0) ? stack[--sp] : 0);
}

/**
 * STOREB opcode - store byte in array (VAR 0x02 = 0xE2)
 *
 * In Z-machine: STOREB array byte-index value
 */
static void op_storeb() {
    uint32_t addr = (uint32_t)zargs[0] + (uint32_t)zargs[1];
    if (addr < 87040) memory[addr] = (zbyte)zargs[2];
}

/**
 * OR opcode - Bitwise OR (2OP 0x08)
 */
static void op_or() {
    zbyte store_var;
    CODE_BYTE(store_var);
    write_variable(store_var, zargs[0] | zargs[1]);
}

/**
 * TEST opcode - Bitwise AND with branch (2OP 0x07)
 * Branch if (flags & mask) == mask.
 */
static void op_test() {
    do_branch((zargs[0] & zargs[1]) == zargs[1]);
}

/**
 * SET_ATTR opcode - Set object attribute (2OP 0x0B)
 * Stub: no-op (no full object tree).
 */
static void op_set_attr() {
    // Would set attribute zargs[1] on object zargs[0]
}

/**
 * CLEAR_ATTR opcode - Clear object attribute (2OP 0x0C)
 * Stub: no-op.
 */
static void op_clear_attr() {
    // Would clear attribute zargs[1] on object zargs[0]
}

/**
 * INSERT_OBJ opcode - Move object into parent (2OP 0x0E)
 * Stub: no-op (no object tree manipulation).
 */
static void op_insert_obj() {
    // Would reparent zargs[0] to zargs[1]
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

        // Track first 50 opcodes for debugging
        if (opcode_track_count < 50) {
            first_opcodes[opcode_track_count++] = opcode;
        }

        // Decode opcode format (from Frotz process.c lines 272-294)
        if (opcode < 0x80) {
            // 2OP opcodes (long form) - like Ruby: a + b, a == b, etc.
            load_operand((opcode & 0x40) ? 2 : 1);
            load_operand((opcode & 0x20) ? 2 : 1);

            zbyte op_num = opcode & 0x1f;
            switch (op_num) {
                case 0x01:  // JE (2OP 0x01)
                    op_je();
                    break;
                case 0x02:  // JL (2OP 0x02)
                    op_jl();
                    break;
                case 0x03:  // JG (2OP 0x03)
                    op_jg();
                    break;
                case 0x04:  // DEC_CHK (2OP 0x04)
                    op_dec_chk();
                    break;
                case 0x05:  // INC_CHK (2OP 0x05)
                    op_inc_chk();
                    break;
                case 0x06:  // JIN (2OP 0x06)
                    op_jin();
                    break;
                case 0x07:  // TEST (2OP 0x07)
                    op_test();
                    break;
                case 0x08:  // OR (2OP 0x08)
                    op_or();
                    break;
                case 0x09:  // AND (2OP 0x09)
                    op_and();
                    break;
                case 0x0A:  // TEST_ATTR (2OP 0x0A)
                    op_test_attr();
                    break;
                case 0x0B:  // SET_ATTR (2OP 0x0B)
                    op_set_attr();
                    break;
                case 0x0C:  // CLEAR_ATTR (2OP 0x0C)
                    op_clear_attr();
                    break;
                case 0x0D:  // STORE (2OP 0x0D)
                    op_store();
                    break;
                case 0x0E:  // INSERT_OBJ (2OP 0x0E)
                    op_insert_obj();
                    break;
                case 0x0F:  // LOADW (2OP 0x0F)
                    op_loadw();
                    break;
                case 0x10:  // LOADB (2OP 0x10)
                    op_loadb();
                    break;
                case 0x11:  // GET_PROP (2OP 0x11)
                    op_get_prop();
                    break;
                case 0x12:  // GET_PROP_ADDR (2OP 0x12) — returns address of property data
                    {
                        zbyte store_var;
                        CODE_BYTE(store_var);
                        write_variable(store_var, 0);  // stub
                    }
                    break;
                case 0x13:  // GET_NEXT_PROP (2OP 0x13) — enumerate properties
                    {
                        zbyte store_var;
                        CODE_BYTE(store_var);
                        write_variable(store_var, 0);  // stub: no next property
                    }
                    break;
                case 0x14:  // ADD (2OP 0x14)
                    op_add();
                    break;
                case 0x15:  // SUB (2OP 0x15)
                    op_sub();
                    break;
                case 0x16:  // MUL (2OP 0x16)
                    op_mul();
                    break;
                case 0x17:  // DIV (2OP 0x17)
                    op_div();
                    break;
                case 0x18:  // MOD (2OP 0x18)
                    op_mod();
                    break;
            }

        } else if (opcode < 0xb0) {
            // 1OP opcodes (short form) - like Ruby: -a, !a, call(a)
            load_operand(opcode >> 4);

            zbyte op_num = opcode & 0x0f;
            switch (op_num) {
                case 0x00:  // JZ (1OP 0x00)
                    op_jz();
                    break;
                case 0x01:  // GET_SIBLING (1OP 0x01)
                    op_get_sibling();
                    break;
                case 0x02:  // GET_CHILD (1OP 0x02) — store + branch
                    op_get_child();
                    break;
                case 0x03:  // GET_PARENT (1OP 0x03) — store only
                    op_get_parent();
                    break;
                case 0x04:  // GET_PROP_LEN (1OP 0x04)
                    op_get_prop_len();
                    break;
                case 0x05:  // INC (1OP 0x05) — no store, no branch
                    op_inc();
                    break;
                case 0x06:  // DEC (1OP 0x06)
                    op_dec();
                    break;
                case 0x07:  // PRINT_ADDR (1OP 0x07)
                    op_print_addr();
                    break;
                case 0x09:  // REMOVE_OBJ (1OP 0x09)
                    op_remove_obj();
                    break;
                case 0x0A:  // PRINT_OBJ (1OP 0x0A)
                    op_print_obj();
                    break;
                case 0x0B:  // RET (1OP 0x0B)
                    op_ret();
                    break;
                case 0x0C:  // JUMP (1OP 0x0C) — unconditional jump
                    op_jump();
                    break;
                case 0x0D:  // PRINT_PADDR (1OP 0x0D)
                    op_print_paddr();
                    break;
                case 0x0E:  // LOAD (1OP 0x0E)
                    op_load();
                    break;
                case 0x0F:  // NOT (1OP 0x0F)
                    op_not();
                    break;
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
            // op_num 0x00-0x1F: 2OP opcodes in VAR encoding (0xC0-0xDF)
            // op_num 0x20-0x3F: true VAR opcodes (0xE0-0xFF)
            switch (op_num) {
                // --- 2OP opcodes in VAR form (0xC0-0xDF) ---
                case 0x01:  // JE in VAR form (0xC1) — up to 4 operands
                    op_je();
                    break;
                case 0x02:  // JL in VAR form (0xC2)
                    op_jl();
                    break;
                case 0x03:  // JG in VAR form (0xC3)
                    op_jg();
                    break;
                case 0x04:  // DEC_CHK in VAR form (0xC4)
                    op_dec_chk();
                    break;
                case 0x05:  // INC_CHK in VAR form (0xC5)
                    op_inc_chk();
                    break;
                case 0x06:  // JIN in VAR form (0xC6)
                    op_jin();
                    break;
                case 0x09:  // AND in VAR form (0xC9)
                    op_and();
                    break;
                case 0x0A:  // TEST_ATTR in VAR form (0xCA)
                    op_test_attr();
                    break;
                case 0x07:  // TEST in VAR form (0xC7 = 2OP 0x07)
                    op_test();
                    break;
                case 0x08:  // OR in VAR form (0xC8 = 2OP 0x08)
                    op_or();
                    break;
                case 0x0B:  // SET_ATTR in VAR form (0xCB = 2OP 0x0B)
                    op_set_attr();
                    break;
                case 0x0C:  // CLEAR_ATTR in VAR form (0xCC = 2OP 0x0C)
                    op_clear_attr();
                    break;
                case 0x0D:  // STORE in VAR form (0xCD = 2OP 0x0D)
                    op_store();
                    break;
                case 0x0E:  // INSERT_OBJ in VAR form (0xCE = 2OP 0x0E)
                    op_insert_obj();
                    break;
                case 0x0F:  // LOADW in VAR form (0xCF = 2OP 0x0F)
                    op_loadw();
                    break;
                case 0x10:  // LOADB in VAR form (0xD0 = 2OP 0x10)
                    op_loadb();
                    break;
                case 0x11:  // GET_PROP in VAR form (0xD1 = 2OP 0x11)
                    op_get_prop();
                    break;
                case 0x14:  // ADD in VAR form (0xD4 = 2OP 0x14)
                    op_add();
                    break;
                case 0x15:  // SUB in VAR form (0xD5 = 2OP 0x15)
                    op_sub();
                    break;
                case 0x16:  // MUL in VAR form (0xD6 = 2OP 0x16)
                    op_mul();
                    break;
                case 0x17:  // DIV in VAR form (0xD7 = 2OP 0x17)
                    op_div();
                    break;
                case 0x18:  // MOD in VAR form (0xD8 = 2OP 0x18)
                    op_mod();
                    break;

                // --- true VAR opcodes (0xE0-0xFF, op_num 0x20-0x3F) ---
                case 0x00:  // 0xC0 rare, handled like CALL
                case 0x20:  // CALL (0xE0)
                    op_call();
                    break;
                case 0x21:  // STOREW (0xE1)
                    op_storew();
                    break;
                case 0x22:  // STOREB (0xE2)
                    op_storeb();
                    break;
                case 0x23:  // PUT_PROP (0xE3)
                    op_put_prop();
                    break;
                case 0x24:  // READ (0xE4) ⭐ CRITICAL FOR GAMEPLAY!
                    op_read();
                    break;
                case 0x25:  // PRINT_CHAR (0xE5)
                    op_print_char();
                    break;
                case 0x26:  // PRINT_NUM (0xE6)
                    op_print_num();
                    break;
                case 0x27:  // RANDOM (0xE7)
                    op_random();
                    break;
                case 0x28:  // PUSH (0xE8)
                    op_push();
                    break;
                case 0x29:  // PULL (0xE9)
                    op_pull();
                    break;
            }
        }
    }
}

/**
 * Helper to output hex digits
 */
static void output_hex_byte(zbyte value) {
    const char* hex = "0123456789ABCDEF";
    if (out_pos < 15000) output[out_pos++] = hex[(value >> 4) & 0xF];
    if (out_pos < 15000) output[out_pos++] = hex[value & 0xF];
}

static void output_hex32(uint32_t value) {
    const char* hex = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        if (out_pos < 15000) output[out_pos++] = hex[(value >> i) & 0xF];
    }
}

/**
 * Output opcode statistics - first 50 opcodes executed
 */
static void output_opcode_stats() {
    const char* h;

    h = "\n=== FIRST 50 OPCODES ===\n";
    while (*h) output[out_pos++] = *h++;

    for (uint32_t i = 0; i < opcode_track_count && i < 50 && out_pos < 14500; i++) {
        if (i % 10 == 0 && i > 0) output[out_pos++] = '\n';

        output[out_pos++] = '0';
        output[out_pos++] = 'x';
        output_hex_byte(first_opcodes[i]);
        output[out_pos++] = ' ';
    }
    output[out_pos++] = '\n';
}

/**
 * Save Z-machine state to buffer for persistence between batches.
 *
 * NOTE: out_pos is NOT saved — each batch starts fresh at out_pos=0 and
 * the Python host accumulates output by concatenating per-batch results.
 * Saving out_pos would cause batch N to write past the start of L1_OUT
 * (which is re-zeroed each kernel invocation), producing garbled output.
 */
static void save_state(ZMachineState* state) {
    state->pc_offset = (uint32_t)(pc - memory);  // Convert pointer to offset
    state->sp = sp;
    state->frame_sp = frame_sp;
    state->finished = finished;
    // out_pos intentionally NOT saved — each batch outputs from position 0

    // Only copy the live portion of the stack (sp entries, not the full 1024).
    // Zork's init uses sp < 10, so copying 1024 entries unconditionally was
    // adding ~1000 extra loop iterations — enough to trip the firmware watchdog.
    for (uint32_t i = 0; i < sp && i < 1024; i++) {
        state->stack[i] = stack[i];
    }

    // Same for call frames: only save the live frame_sp entries.
    for (uint32_t i = 0; i < frame_sp && i < 64; i++) {
        state->frames[i] = frames[i];
        if (state->frames[i].ret_pc != nullptr) {
            state->frames[i].ret_pc = (zbyte*)(state->frames[i].ret_pc - memory);
        }
    }
}

/**
 * Load Z-machine state from buffer to resume execution.
 *
 * NOTE: out_pos is NOT restored — caller sets it to 0 before this call,
 * so each batch writes fresh output starting at position 0.
 */
static void load_state(const ZMachineState* state) {
    pc = memory + state->pc_offset;  // Convert offset back to pointer
    sp = state->sp;
    frame_sp = state->frame_sp;
    finished = state->finished;
    // out_pos intentionally NOT restored — stays at 0 (set by kernel_main)

    // Only restore the live stack entries saved by save_state().
    for (uint32_t i = 0; i < state->sp && i < 1024; i++) {
        stack[i] = state->stack[i];
    }

    // Only restore the live call frames saved by save_state().
    for (uint32_t i = 0; i < state->frame_sp && i < 64; i++) {
        frames[i] = state->frames[i];
        if (frames[i].ret_pc != nullptr) {
            frames[i].ret_pc = memory + (uint32_t)frames[i].ret_pc;
        }
    }
}

/**
 * Kernel main - BATCHED EXECUTION with state persistence
 * Runs 100 instructions, saves state to DRAM for next batch
 *
 * ARCHITECTURE FOR STREAMING EXECUTION:
 * =====================================
 * Problem: Running interpret(10) works, interpret(150+) locks up device
 * Solution: Run multiple kernel invocations, each doing 100 instructions
 *
 * Required additions (TODO next session):
 * 1. Add STATE_DRAM_ADDR define (optional, for batched mode)
 * 2. At start: if (STATE_DRAM_ADDR exists && state.instruction_count > 0)
 *       load_state() to resume from previous batch
 * 3. Run interpret(10)
 * 4. save_state() with updated instruction_count
 * 5. Write state back to STATE_DRAM via NoC
 *
 * Host responsibilities:
 * - Create 3rd DRAM buffer for ZMachineState (~10KB)
 * - Loop: run kernel, check if state.finished, repeat until done
 * - Accumulate output from each batch
 *
 * This architecture works WITH the hardware (short kernels) instead of
 * against it (long loops). Proven reliable at interpret(10) per batch.
 */
void kernel_main() {
    // L1 memory layout — must not overlap.
    // Addresses are fixed (not runtime-configurable) to keep the kernel simple.
    //
    //   0x10000  L1_GAME    —  87040 B  game data (zork1.z3, padded to 0x15400)
    //   0x26000  L1_STACK   —   2048 B  Z-machine stack (1024 × zword = 2 KB)
    //   0x26800  L1_FRAMES  —   2432 B  call frames (64 × sizeof(Frame) ≈ 38 B each)
    //   0x27200  L1_OPCODES —     64 B  opcode trace buffer (first_opcodes[50])
    //   0x30000  L1_OUT     —  16384 B  output text buffer
    //   0x34000  L1_INPUT   —   1024 B  input command (null-terminated)
    //   0x50000  L1_STATE   —  ~10 KB   ZMachineState (only when STATE_DRAM_ADDR defined)
    //
    // Gap check: L1_STACK (0x26000) starts after game data ends (0x10000+0x15400=0x25400).
    // 0x26000 - 0x25400 = 0xC00 = 3072 bytes of headroom. ✓
    constexpr uint32_t L1_GAME    = 0x10000;
    constexpr uint32_t L1_STACK   = 0x26000;   // stack[1024] → 2048 bytes
    constexpr uint32_t L1_FRAMES  = 0x26800;   // frames[64]  → sizeof(Frame)*64 bytes
    constexpr uint32_t L1_OPCODES = 0x27200;   // first_opcodes[50] → 64 bytes (padded)
    constexpr uint32_t L1_OUT     = 0x30000;
    constexpr uint32_t L1_INPUT   = 0x34000;
    constexpr uint32_t GAME_SIZE  = 87040;
    constexpr uint32_t INPUT_SIZE = 1024;

    // Step 0 (L1-variant): Initialise large-array pointers to their L1 addresses.
    // This MUST happen before any code that touches stack[], frames[], or first_opcodes[].
    // (Replaces the static array declarations that would overflow .bss in newer firmware.)
    stack        = reinterpret_cast<zword*>(L1_STACK);
    frames       = reinterpret_cast<Frame*>(L1_FRAMES);
    first_opcodes = reinterpret_cast<zbyte*>(L1_OPCODES);

    // Step 1: Issue all DRAM→L1 reads in one pass, then a single barrier.
    // Previously each 4KB game chunk had its own barrier (22 barriers for the
    // 87KB game file alone, 23 total with input). That serial round-trip overhead
    // consumed most of the firmware watchdog budget before interpret() even ran.
    // Batched reads reduce 23 barriers to 1 — the NOC can service all in-flight
    // reads concurrently (Blackhole supports 32 per direction per endpoint).
    constexpr uint32_t CHUNK_SIZE = 4096;
    uint32_t offset = 0;
    while (offset < GAME_SIZE) {
        uint32_t chunk = (offset + CHUNK_SIZE > GAME_SIZE) ? (GAME_SIZE - offset) : CHUNK_SIZE;
        uint64_t src = get_noc_addr(0, 0, GAME_DRAM_ADDR + offset);
        noc_async_read(src, L1_GAME + offset, chunk);
        // No per-chunk barrier — batch all reads and wait once below
        offset += chunk;
    }

    // Input read batched with game reads — single barrier covers both
    uint64_t input_src = get_noc_addr(0, 0, INPUT_DRAM_ADDR);
    noc_async_read(input_src, L1_INPUT, INPUT_SIZE);
    noc_async_read_barrier();  // one barrier covers all 22 game chunks + input

    memory = (zbyte*)L1_GAME;
    output = (char*)L1_OUT;
    input = (char*)L1_INPUT;

    // Initialize opcode tracking
    opcode_track_count = 0;

    // Initialize global Z-machine constants
    abbrev_table = read_word(0x18);      // Abbreviations table
    global_vars_addr = read_word(0x0C);  // Global variables table
    dictionary_addr = read_word(0x08);   // Dictionary table

    // Always reset out_pos = 0 so this batch's output fills the buffer from
    // the beginning. The Python host reads the buffer after each batch and
    // concatenates results. This avoids writing past L1_OUT (re-zeroed each
    // kernel invocation) and keeps the output logic simple.
    out_pos = 0;

#ifdef STATE_DRAM_ADDR
    // BATCHED EXECUTION MODE: Load previous state if exists.
    //
    // State buffer layout in the 32 KB DRAM tensor (STATE_DRAM_ADDR):
    //   [0 .. STRUCT_SIZE-1]              : ZMachineState struct (PC, stack, frames, …)
    //   [DYN_OFFSET .. DYN_OFFSET+dyn-1]  : Dynamic game memory snapshot
    //                                        (bytes 0..read_word(0x0E)-1 of game memory)
    //
    // WHY dynamic memory? Each batch reloads the original game file from DRAM into L1,
    // which overwrites any globals/objects/flags modified by previous batches. Saving the
    // "dynamic" segment (everything below the static memory base stored in header[0x0E])
    // and restoring it after reload ensures global variables and object state persist
    // correctly across batches.
    constexpr uint32_t L1_STATE  = 0x50000;

    // ZMachineState struct size, DYN_OFFSET = first 32-byte boundary after the struct.
    constexpr uint32_t STRUCT_SIZE = sizeof(ZMachineState);
    constexpr uint32_t DYN_OFFSET  = ((STRUCT_SIZE + 31) / 32) * 32;

    // Read the full 32 KB state tensor — covers struct + up to ~27 KB of dynamic memory.
    // Dynamic memory for Zork 1.z3 is 11282 bytes; total = ~16434 bytes, within 32 KB.
    constexpr uint32_t STATE_READ_SIZE = 32 * 1024;

    // Read state from DRAM into L1 — single barrier (game+input reads already done above)
    uint64_t state_dram_noc_addr = get_noc_addr(0, 0, STATE_DRAM_ADDR);
    noc_async_read(state_dram_noc_addr, L1_STATE, STATE_READ_SIZE);
    noc_async_read_barrier();

    ZMachineState* state = (ZMachineState*)L1_STATE;

    if (state->instruction_count > 0) {
        // Resume: restore interpreter state from previous batch
        // out_pos stays 0 (reset above) — fresh output buffer for this batch
        load_state(state);

        // Restore dynamic game memory (global vars, object attributes, flags).
        // The game file reload above reset memory[0..dyn_size-1] to the original ROM;
        // overwrite it with the state saved by the previous batch.
        uint32_t dyn_size = (uint32_t)(((uint32_t)memory[0x0E] << 8) | (uint32_t)memory[0x0F]);
        zbyte* dyn_src = reinterpret_cast<zbyte*>(L1_STATE + DYN_OFFSET);
        for (uint32_t i = 0; i < dyn_size; i++) {
            memory[i] = dyn_src[i];
        }
    } else {
        // First batch: initialize the Z-machine interpreter from scratch
        zword initial_pc = read_word(0x06);   // Initial PC from header byte 0x06
        sp = 0;
        frame_sp = 0;
        finished = false;
        pc = memory + initial_pc;
        // instruction_count is already 0 in the zero-initialised state tensor
    }
#else
    // SINGLE-SHOT MODE: Always initialize fresh — no state persistence.
    // Use this for a single 40-instruction probe (testing/debugging).
    zword initial_pc = read_word(0x06);
    sp = 0;
    frame_sp = 0;
    finished = false;
    pc = memory + initial_pc;
#endif

    // Run interpreter. Note: firmware watchdog limits execution time.
    // QB2/Blackhole with new TT-Lang firmware (build 6745986192171285359) has a
    // shorter watchdog than the original Blackhole session (build from Jan 2026).
    // Binary search for stable instruction count:
    //   interpret(10)  = completes cleanly, no output (not enough instructions)
    //   interpret(100) = firmware watchdog triggered (hangs)
    // Instruction count per batch: 10 (conservative, survives PRINT opcode).
    // Empirical watchdog budget on QB2 / firmware 6745986192171285359:
    //   interpret(10)  = completes reliably — safe even when PRINT fires because
    //                    the Z-string decode loop completes within the watchdog window
    //   interpret(20)  = HANGS at batch 3: PRINT Z-string decode adds overhead
    //                    beyond the 20-iteration count, tips the watchdog
    //   interpret(30)  = HANGS at batch 3 for same reason (worse)
    //   interpret(40)  = was watchdog-safe WITHOUT state I/O (no PRINT)
    //   interpret(45+) = firmware watchdog (hang)
    // 10 batches × 10 = 100 instructions — sufficient for "West of House" opening text.
    interpret(10);

    output[out_pos++] = '\0';

#ifdef STATE_DRAM_ADDR
    // Save updated state back to DRAM for the next batch.
    // We executed 10 instructions in this invocation (interpret(10) above).
    state->instruction_count += 10;
    save_state(state);

    // Save dynamic game memory (global vars, object attributes, flags) after the struct.
    // dyn_size = header[0x0E] big-endian word = static memory base = 11282 for Zork 1.z3.
    // Saving bytes 0..dyn_size-1 ensures the next batch restores them after reloading the ROM.
    {
        uint32_t dyn_size = (uint32_t)(((uint32_t)memory[0x0E] << 8) | (uint32_t)memory[0x0F]);
        zbyte* dyn_dst = reinterpret_cast<zbyte*>(L1_STATE + DYN_OFFSET);
        for (uint32_t i = 0; i < dyn_size; i++) {
            dyn_dst[i] = memory[i];
        }
        // Write struct + dynamic memory to DRAM (rounded up to 32-byte alignment for NoC)
        uint32_t total_state = DYN_OFFSET + dyn_size;
        uint32_t state_write_size = ((total_state + 31) / 32) * 32;
        noc_async_write(L1_STATE, state_dram_noc_addr, state_write_size);
        noc_async_write_barrier();
    }
#endif

    // Step 2: Use NoC to copy output from L1 to DRAM
    uint32_t output_size = ((out_pos + 31) / 32) * 32;  // Round to 32-byte alignment
    uint64_t output_dram_noc_addr = get_noc_addr(0, 0, OUTPUT_DRAM_ADDR);
    noc_async_write(L1_OUT, output_dram_noc_addr, output_size);
    noc_async_write_barrier();

    // Done! Output and state transferred from L1 to DRAM
}
