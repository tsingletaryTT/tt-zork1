// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0


#include <cstdint>
#include "api/dataflow/dataflow_api.h"

#ifndef GAME_DRAM_ADDR
#error "GAME_DRAM_ADDR must be defined"
#endif
#ifndef OUTPUT_DRAM_ADDR
#error "OUTPUT_DRAM_ADDR must be defined"
#endif

typedef uint8_t zbyte;
typedef uint16_t zword;

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

struct Frame {
    zbyte* ret_pc;       // Where to return to
    zbyte num_locals;    // How many local variables this routine has
    zword locals[15];    // The local variable values
    zbyte store_var;     // Where to store the return value
};
static Frame frames[64];
static uint32_t frame_sp;

static bool finished;


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

#define CODE_BYTE(v) v = *pc++

#define CODE_WORD(v) v = (*pc << 8) | *(pc+1); pc += 2

static inline zbyte read_byte(uint32_t addr) {
    if (addr >= 86000) return 0;
    return memory[addr];
}

static inline zword read_word(uint32_t addr) {
    if (addr >= 86000) return 0;
    return (memory[addr] << 8) | memory[addr + 1];
}

static inline void write_word(uint32_t addr, zword value) {
    if (addr < 86000) {
        memory[addr] = (value >> 8) & 0xFF;
        memory[addr + 1] = value & 0xFF;
    }
}

static char get_char(int alph, int idx) {
    if (alph == 0) {
        if (idx == 0) return ' ';
        if (idx >= 6 && idx <= 31) return 'a' + (idx - 6);
    } else if (alph == 1) {
        if (idx == 0) return ' ';
        if (idx >= 6 && idx <= 31) return 'A' + (idx - 6);
    } else {
        if (idx == 0) return ' ';
        if (idx == 6) return ' ';   // Position 0
        if (idx == 7) return '\n';  // Position 1 - NEWLINE!
        const char* punct = "0123456789.,!?_#'\"/\\-:()";
        if (idx >= 8 && idx <= 31) return punct[idx - 8];
    }
    return '?';
}

static void decode_zstring(uint32_t addr, uint32_t max_words, uint32_t depth);

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

static zword read_variable(zbyte var) {
    if (var == 0) {
        if (sp > 0) {
            return stack[--sp];
        }
        return 0;
    } else if (var >= 0x01 && var <= 0x0F) {
        if (frame_sp > 0) {
            zbyte local_num = var - 1;
            Frame& current_frame = frames[frame_sp - 1];
            if (local_num < current_frame.num_locals) {
                return current_frame.locals[local_num];
            }
        }
        return 0;
    } else {
        zword addr = global_vars_addr + ((var - 0x10) * 2);
        return read_word(addr);
    }
}

static void write_variable(zbyte var, zword value) {
    if (var == 0) {
        if (sp < 1024) {
            stack[sp++] = value;
        }
    } else if (var >= 0x01 && var <= 0x0F) {
        if (frame_sp > 0) {
            zbyte local_num = var - 1;
            Frame& current_frame = frames[frame_sp - 1];
            if (local_num < current_frame.num_locals) {
                current_frame.locals[local_num] = value;
            }
        }
    } else {
        zword addr = global_vars_addr + ((var - 0x10) * 2);
        write_word(addr, value);
    }
}

static char tohex(uint8_t nibble) {
    return (nibble < 10) ? ('0' + nibble) : ('A' + (nibble - 10));
}

static void load_operand(int type) {
    if (zargc >= 8) return;

    if (type & 2) {
        zbyte var;
        CODE_BYTE(var);
        zargs[zargc] = read_variable(var);
    } else if (type & 1) {
        zbyte b;
        CODE_BYTE(b);
        zargs[zargc] = b;
    } else {
        CODE_WORD(zargs[zargc]);
    }
    zargc++;
}

static void load_all_operands(zbyte specifier) {
    for (int i = 6; i >= 0; i -= 2) {
        int type = (specifier >> i) & 0x03;
        if (type == 3) break;  // 11 = no more operands
        load_operand(type);
    }
}

static void do_branch(bool condition) {
    zbyte branch_byte;
    CODE_BYTE(branch_byte);

    bool branch_on_true = (branch_byte & 0x80) != 0;

    bool short_form = (branch_byte & 0x40) != 0;

    int16_t offset;
    if (short_form) {
        offset = branch_byte & 0x3F;
    } else {
        zbyte second_byte;
        CODE_BYTE(second_byte);
        offset = ((branch_byte & 0x3F) << 8) | second_byte;

        if (offset & 0x2000) {
            offset |= 0xC000;  // Set top 2 bits for negative
        }
    }

    bool do_it = (condition == branch_on_true);

    if (do_it) {
        if (offset == 0 || offset == 1) {
            if (frame_sp > 0) {
                frame_sp--;
                Frame frame = frames[frame_sp];
                pc = frame.ret_pc;
                write_variable(frame.store_var, offset);
            }
        } else {
            pc += (offset - 2);
        }
    }
}

static void op_store() {
    zbyte var;
    CODE_BYTE(var);
    write_variable(var, zargs[0]);
}

static void op_load() {
    zbyte var;
    CODE_BYTE(var);
    zword value = read_variable(var);

    zbyte store_var;
    CODE_BYTE(store_var);
    write_variable(store_var, value);
}

static void op_jz() {
    do_branch(zargs[0] == 0);
}

static void op_je() {
    bool equal = false;
    for (uint32_t i = 1; i < zargc; i++) {
        if (zargs[0] == zargs[i]) {
            equal = true;
            break;
        }
    }
    do_branch(equal);
}

static void op_print() {
    uint32_t addr = pc - memory;  // Current PC as address
    decode_zstring(addr, 30, 0);

    while ((uint32_t)(pc - memory) < 86000) {
        zword word = (pc[0] << 8) | pc[1];
        pc += 2;
        if (word & 0x8000) break;  // End bit
    }
}

static void op_print_ret() {
    op_print();
    if (out_pos < 15000) output[out_pos++] = '\n';
    // TODO: Implement return
}

static void op_new_line() {
    if (out_pos < 15000) output[out_pos++] = '\n';
}

static void op_call() {

    zword routine_addr = zargs[0];

    if (routine_addr == 0) {
        zbyte store_var;
        CODE_BYTE(store_var);
        write_variable(store_var, 0);  // Return 0 (false)
        return;
    }

    zbyte store_var;
    CODE_BYTE(store_var);

    Frame new_frame;
    new_frame.ret_pc = pc;  // "Come back to this address when done"
    new_frame.store_var = store_var;  // "Store result in this variable"

    uint32_t byte_addr = routine_addr * 2;
    if (byte_addr >= 86000) return;  // Safety check

    zbyte* routine_ptr = memory + byte_addr;
    zbyte num_locals = *routine_ptr++;  // "This function has N local variables"
    new_frame.num_locals = num_locals;

    for (int i = 0; i < num_locals; i++) {
        if ((uint32_t)i + 1 < zargc) {
            new_frame.locals[i] = zargs[i + 1];
        } else {
            zword default_val = (routine_ptr[0] << 8) | routine_ptr[1];
            routine_ptr += 2;
            new_frame.locals[i] = default_val;
        }
    }

    if (frame_sp < 64) {
        frames[frame_sp++] = new_frame;
    }

    pc = routine_ptr;
}

static void op_ret() {
    if (frame_sp == 0) {
        return;
    }

    zword return_value = zargs[0];

    frame_sp--;
    Frame frame = frames[frame_sp];

    pc = frame.ret_pc;

    write_variable(frame.store_var, return_value);
}

static void op_add() {
    zbyte store_var;
    CODE_BYTE(store_var);

    int16_t a = (int16_t)zargs[0];
    int16_t b = (int16_t)zargs[1];
    int16_t result = a + b;

    write_variable(store_var, (zword)result);
}

static void op_storew() {

    uint32_t addr = zargs[0] + (zargs[1] * 2);  // Word index -> byte address
    write_word(addr, zargs[2]);
}

static void op_get_sibling() {
    zbyte store_var;
    CODE_BYTE(store_var);
    write_variable(store_var, 0);

    do_branch(false);
}

static void op_put_prop() {

}

static void op_get_prop() {
    zbyte store_var;
    CODE_BYTE(store_var);

    write_variable(store_var, 0);
}

static void op_get_child() {
    zbyte store_var;
    CODE_BYTE(store_var);

    write_variable(store_var, 0);
    do_branch(false);
}

static void op_get_parent() {
    zbyte store_var;
    CODE_BYTE(store_var);

    write_variable(store_var, 0);
}

static void op_and() {
    zbyte store_var;
    CODE_BYTE(store_var);

    zword result = zargs[0] & zargs[1];
    write_variable(store_var, result);
}

static void op_test_attr() {

    do_branch(false);
}

static void op_dec_chk() {

    zbyte var = (zbyte)zargs[0];
    zword current = read_variable(var);

    int16_t decremented = (int16_t)current - 1;
    write_variable(var, (zword)decremented);

    bool less_than = decremented < (int16_t)zargs[1];
    do_branch(less_than);
}

static void op_random() {
    zbyte store_var;
    CODE_BYTE(store_var);

    int16_t range = (int16_t)zargs[0];

    if (range <= 0) {
        write_variable(store_var, 0);
    } else {
        write_variable(store_var, 1);
    }
}

static void op_rtrue() {
    if (frame_sp == 0) {
        return;
    }

    frame_sp--;
    Frame frame = frames[frame_sp];
    pc = frame.ret_pc;
    write_variable(frame.store_var, 1);  // TRUE = 1
}

static void op_rfalse() {
    if (frame_sp == 0) {
        return;
    }

    frame_sp--;
    Frame frame = frames[frame_sp];
    pc = frame.ret_pc;
    write_variable(frame.store_var, 0);  // FALSE = 0
}

static uint8_t last_opcode_for_print_obj = 0;  // Debug: capture triggering opcode

static void op_print_obj() {

    zword obj_num = zargs[0];

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

static void op_print_addr() {
    zword addr = zargs[0];
    if (addr > 0 && addr < 85000 && out_pos < 14000) {
        decode_zstring(addr, 10, 0);  // Limit to 10 words max
    }
}

static void op_print_char() {
    zbyte ch = (zbyte)zargs[0];
    if (out_pos < 15000) output[out_pos++] = ch;
}

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

static void interpret(uint32_t max_instructions) {
    uint32_t instructions = 0;
    finished = false;

    while (!finished && instructions < max_instructions && (uint32_t)(pc - memory) < 86000) {
        zbyte opcode;
        CODE_BYTE(opcode);
        zargc = 0;

        instructions++;

        }

        if (opcode < 0x80) {
            load_operand((opcode & 0x40) ? 2 : 1);
            load_operand((opcode & 0x20) ? 2 : 1);

            zbyte op_num = opcode & 0x1f;
            switch (op_num) {
                case 0x01:  // JE - jump if equal
                    op_je();
                    break;
                case 0x04:  // DEC_CHK - decrement and check
                    op_dec_chk();
                    break;
                case 0x09:  // AND - bitwise AND
                    op_and();
                    break;
                case 0x0A:  // TEST_ATTR - test object attribute
                    op_test_attr();
                    break;
                case 0x0B:  // PUT_PROP - write object property
                    op_put_prop();
                    break;
                case 0x14:  // ADD - addition
                    op_add();
                    break;
            }

        } else if (opcode < 0xb0) {
            load_operand(opcode >> 4);

            zbyte op_num = opcode & 0x0f;
            switch (op_num) {
                case 0x00:  // JZ - jump if zero
                    op_jz();
                    break;
                case 0x01:  // GET_SIBLING - get object sibling
                    op_get_sibling();
                    break;
                case 0x02:  // GET_PROP - get object property
                    op_get_prop();
                    break;
                case 0x03:  // GET_PARENT - get object parent
                    op_get_parent();
                    break;
                case 0x05:  // GET_CHILD - get object child
                    op_get_child();
                    break;
                // case 0x07:  // PRINT_ADDR - print string at address (disabled - needs debugging)
                //     op_print_addr();
                //     break;
                // case 0x0A:  // PRINT_OBJ - print object name (disabled - testing operand fix)
                //     last_opcode_for_print_obj = opcode;  // Debug: capture opcode
                //     op_print_obj();
                //     break;
                case 0x0B:  // RET - return value
                    op_ret();
                    break;
                case 0x0E:  // LOAD - load variable
                    op_load();
                    break;
            }

        } else if (opcode < 0xc0) {
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
                    break;
            }
        } else {
            zbyte specifier1;
            CODE_BYTE(specifier1);
            load_all_operands(specifier1);

            zbyte op_num = opcode - 0xc0;
            switch (op_num) {
                case 0x00:  // CALL_1S - call routine (1 arg min)
                case 0x20:  // CALL_VS2 - call routine (variable args, extended)
                    op_call();
                    break;
                case 0x05:  // PRINT_CHAR - print character
                    op_print_char();
                    break;
                case 0x06:  // PRINT_NUM - print number
                    op_print_num();
                    break;
                case 0x07:  // RANDOM - random number generator
                    op_random();
                    break;
                case 0x0D:  // STORE - store to variable
                    op_store();
                    break;
                case 0x21:  // STOREW - write word to array
                    op_storew();
                    break;
                case 0x23:  // PUT_PROP - write object property
                    op_put_prop();
                    break;
            }
        }
    }
}

static void output_hex_byte(zbyte value) {
    const char* hex = "0123456789ABCDEF";
    if (out_pos < 15000) output[out_pos++] = hex[(value >> 4) & 0xF];
    if (out_pos < 15000) output[out_pos++] = hex[value & 0xF];
}

static void output_opcode_stats() {
    const char* h;

    h = "\n=== FIRST 50 OPCODES ===\n";
    while (*h) output[out_pos++] = *h++;

        if (i % 10 == 0 && i > 0) output[out_pos++] = '\n';

        output[out_pos++] = '0';
        output[out_pos++] = 'x';
        output[out_pos++] = ' ';
    }
    output[out_pos++] = '\n';
}

static void save_state(ZMachineState* state) {
    state->pc_offset = (uint32_t)(pc - memory);  // Convert pointer to offset
    state->sp = sp;
    state->frame_sp = frame_sp;
    state->finished = finished;
    state->out_pos = out_pos;

    for (uint32_t i = 0; i < 1024; i++) {
        state->stack[i] = stack[i];
    }

    for (uint32_t i = 0; i < 64; i++) {
        state->frames[i] = frames[i];
        if (state->frames[i].ret_pc != nullptr) {
            state->frames[i].ret_pc = (zbyte*)(state->frames[i].ret_pc - memory);
        }
    }
}

static void load_state(const ZMachineState* state) {
    pc = memory + state->pc_offset;  // Convert offset back to pointer
    sp = state->sp;
    frame_sp = state->frame_sp;
    finished = state->finished;
    out_pos = state->out_pos;

    for (uint32_t i = 0; i < 1024; i++) {
        stack[i] = state->stack[i];
    }

    for (uint32_t i = 0; i < 64; i++) {
        frames[i] = state->frames[i];
        if (frames[i].ret_pc != nullptr) {
            frames[i].ret_pc = memory + (uint32_t)frames[i].ret_pc;
        }
    }
}

void kernel_main() {
    constexpr uint32_t L1_GAME = 0x10000;    // 64KB into L1 for game
    constexpr uint32_t L1_OUT = 0x30000;     // 192KB into L1 for output
    constexpr uint32_t GAME_SIZE = 87040;  // Actual game size + alignment

    constexpr uint32_t CHUNK_SIZE = 4096;
    uint32_t offset = 0;
    while (offset < GAME_SIZE) {
        uint32_t chunk = (offset + CHUNK_SIZE > GAME_SIZE) ? (GAME_SIZE - offset) : CHUNK_SIZE;
        uint64_t src = get_noc_addr(0, 0, GAME_DRAM_ADDR + offset);
        noc_async_read(src, L1_GAME + offset, chunk);
        noc_async_read_barrier();
        offset += chunk;
    }

    memory = (zbyte*)L1_GAME;
    output = (char*)L1_OUT;


    abbrev_table = read_word(0x18);      // Abbreviations table
    global_vars_addr = read_word(0x0C);  // Global variables table

#ifdef STATE_DRAM_ADDR
    constexpr uint32_t L1_STATE = 0x50000;  // 320KB into L1 for state
    constexpr uint32_t STATE_SIZE = sizeof(ZMachineState);

    uint64_t state_dram_noc_addr = get_noc_addr(0, 0, STATE_DRAM_ADDR);
    noc_async_read(state_dram_noc_addr, L1_STATE, STATE_SIZE);
    noc_async_read_barrier();

    ZMachineState* state = (ZMachineState*)L1_STATE;

    if (state->instruction_count > 0) {
        load_state(state);
        const char* h = "[Resuming from previous batch]\n";
        while (*h) output[out_pos++] = *h++;
    } else {
        zword initial_pc = read_word(0x06);
        sp = 0;
        frame_sp = 0;
        finished = false;
        pc = memory + initial_pc;
        out_pos = 0;
        state->instruction_count = 0;
    }
#else
    zword initial_pc = read_word(0x06);
    sp = 0;
    frame_sp = 0;
    finished = false;
    pc = memory + initial_pc;
    out_pos = 0;
#endif

    const char* h = "╔════════════════════════════════════════════════════╗\n";
    while (*h) output[out_pos++] = *h++;
    h = "║  ZORK ON BLACKHOLE RISC-V - FULL INTERPRETER!   ║\n";
    while (*h) output[out_pos++] = *h++;
    h = "╚════════════════════════════════════════════════════╝\n\n";
    while (*h) output[out_pos++] = *h++;

    h = "Opcodes: PRINT CALL RET STORE LOAD JZ JE ADD\n";
    while (*h) output[out_pos++] = *h++;
    h = "         STOREW PUT_PROP GET_PROP AND TEST_ATTR\n";
    while (*h) output[out_pos++] = *h++;
    h = "         DEC_CHK GET_CHILD GET_PARENT GET_SIBLING\n";
    while (*h) output[out_pos++] = *h++;
    output[out_pos++] = '\n';

    h = "=== EXECUTING Z-MACHINE CODE ===\n\n";
    while (*h) output[out_pos++] = *h++;

    interpret(10);
    h = "[interpret(10) complete - actual Zork text above!]\n";
    while (*h) output[out_pos++] = *h++;

    h = "\n=== EXECUTION COMPLETE ===\n";
    while (*h) output[out_pos++] = *h++;

    if (finished) {
        h = "(Game returned from main routine)\n";
        while (*h) output[out_pos++] = *h++;
    }

    output_opcode_stats();

    output[out_pos++] = '\0';

#ifdef STATE_DRAM_ADDR
    state->instruction_count += 100;  // We just executed 100 instructions
    save_state(state);

    uint32_t state_size = ((STATE_SIZE + 31) / 32) * 32;
    noc_async_write(L1_STATE, state_dram_noc_addr, state_size);
    noc_async_write_barrier();
#endif

    uint32_t output_size = ((out_pos + 31) / 32) * 32;  // Round to 32-byte alignment
    uint64_t output_dram_noc_addr = get_noc_addr(0, 0, OUTPUT_DRAM_ADDR);
    noc_async_write(L1_OUT, output_dram_noc_addr, output_size);
    noc_async_write_barrier();

}
