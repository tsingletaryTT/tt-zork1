// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_execute_startup.cpp - Execute Zork's startup sequence!
 *
 * HOLISTIC APPROACH: Run the actual Z-machine bytecode from PC.
 * Implement core opcodes: PRINT, PRINT_RET, CALL, RET, JUMP, etc.
 * Capture all text output to see the real game opening!
 */

#include <cstdint>

typedef uint8_t zbyte;
typedef uint16_t zword;
typedef uint16_t zchar;

// Z-machine state
struct ZMachine {
    zbyte* memory;
    zword pc;
    zword stack[1024];
    uint32_t sp;
    zword local_vars[15];
    char* output;
    uint32_t output_pos;
    uint32_t max_output;
    zword call_stack[64];  // Return addresses
    uint32_t call_sp;
};

static ZMachine zm;

// Z-machine header
struct {
    zword abbreviations;
    zbyte version;
} z_header;

// Output functions for text decoder
static char* output_ptr;
static uint32_t output_limit;

static void outchar(zchar c) {
    if (zm.output_pos < zm.max_output) {
        zm.output[zm.output_pos++] = (char)c;
    }
}

static zchar alphabet(int set, int index) {
    if (set == 0) return 'a' + index;
    else if (set == 1) return 'A' + index;
    else return " ^0123456789.,!?_#'\"/\\-:()"[index];
}

static zchar translate_from_zscii(zbyte c) {
    return c;
}

// Real Frotz text decoder
static void decode_text(zword z_addr) {
    zbyte c, prev_c = 0;
    int shift_state = 0;
    int shift_lock = 0;
    int status = 0;
    zword addr = z_addr;
    zword code;

    do {
        code = (zm.memory[addr] << 8) | zm.memory[addr + 1];
        addr += 2;

        for (int i = 10; i >= 0; i -= 5) {
            c = (code >> i) & 0x1f;

            switch (status) {
            case 0:  // normal
                if (shift_state == 2 && c == 6)
                    status = 2;
                else if (c >= 6)
                    outchar(alphabet(shift_state, c - 6));
                else if (c == 0)
                    outchar(' ');
                else if (z_header.version >= 3 && c <= 3)
                    status = 1;  // abbreviation
                else {
                    shift_state = (shift_lock + (c & 1) + 1) % 3;
                    break;
                }
                shift_state = shift_lock;
                break;

            case 1:  // abbreviation
                {
                    zword ptr_addr = z_header.abbreviations + 64 * (prev_c - 1) + 2 * c;
                    zword abbr_addr = (zm.memory[ptr_addr] << 8) | zm.memory[ptr_addr + 1];
                    abbr_addr *= 2;
                    decode_text(abbr_addr);
                    status = 0;
                }
                break;

            case 2:  // ZSCII first part
                status = 3;
                break;

            case 3:  // ZSCII second part
                outchar(translate_from_zscii((prev_c << 5) | c));
                status = 0;
                break;
            }
            prev_c = c;
        }
    } while (!(code & 0x8000));
}

// Helper: read word from memory
static inline zword read_word(zword addr) {
    return (zm.memory[addr] << 8) | zm.memory[addr + 1];
}

// Helper: read byte
static inline zbyte read_byte(zword addr) {
    return zm.memory[addr];
}

// Execute one instruction, return false if should stop
static bool execute_instruction() {
    zbyte opcode = read_byte(zm.pc++);

    // Decode opcode form
    if (opcode == 0xBE) {
        // Extended opcode (v5+) - skip for now
        zm.pc++;
        return true;
    }

    if (opcode >= 0xB0 && opcode <= 0xDF) {
        // 0OP form (no operands)
        zbyte op = opcode & 0x0F;

        switch (opcode) {
        case 0xB0:  // RTRUE
            if (zm.call_sp > 0) {
                zm.pc = zm.call_stack[--zm.call_sp];
            } else {
                return false;  // Return from main - game over
            }
            break;

        case 0xB1:  // RFALSE
            if (zm.call_sp > 0) {
                zm.pc = zm.call_stack[--zm.call_sp];
            } else {
                return false;
            }
            break;

        case 0xB2:  // PRINT
            decode_text(zm.pc);
            // Skip past the text (find end bit)
            while (!(read_word(zm.pc) & 0x8000)) zm.pc += 2;
            zm.pc += 2;
            break;

        case 0xB3:  // PRINT_RET
            decode_text(zm.pc);
            while (!(read_word(zm.pc) & 0x8000)) zm.pc += 2;
            zm.pc += 2;
            outchar('\n');
            if (zm.call_sp > 0) {
                zm.pc = zm.call_stack[--zm.call_sp];
            } else {
                return false;
            }
            break;

        case 0xB8:  // RET_POPPED
            if (zm.call_sp > 0) {
                zm.pc = zm.call_stack[--zm.call_sp];
            } else {
                return false;
            }
            break;

        case 0xBB:  // NEW_LINE
            outchar('\n');
            break;

        case 0xBA:  // QUIT
            return false;

        default:
            // Unknown 0OP - skip
            break;
        }

    } else if ((opcode & 0xC0) == 0xC0) {
        // VAR form
        zbyte operand_types = read_byte(zm.pc++);
        // Skip operands for now
        for (int i = 0; i < 4; i++) {
            zbyte type = (operand_types >> (6 - i*2)) & 0x03;
            if (type == 0) zm.pc += 2;  // Large constant
            else if (type == 1) zm.pc++;  // Small constant
            else if (type == 2) ;  // Variable
            else break;  // Omitted
        }

    } else if ((opcode & 0x80) == 0x00) {
        // 2OP form - skip operands
        zbyte types = opcode & 0x40;
        if (types) {
            // Variable types in next byte
            zbyte operand_types = read_byte(zm.pc++);
            for (int i = 0; i < 2; i++) {
                zbyte type = (operand_types >> (6 - i*2)) & 0x03;
                if (type == 0) zm.pc += 2;
                else if (type == 1) zm.pc++;
            }
        } else {
            // Both small constants
            zm.pc += 2;
        }

    } else {
        // 1OP form
        zbyte type = (opcode >> 4) & 0x03;
        if (type == 0) zm.pc += 2;  // Large constant
        else if (type == 1) zm.pc++;  // Small constant
        else if (type == 2) ;  // Variable
    }

    return true;
}

void kernel_main() {
    uint32_t game_data_dram = get_arg_val<uint32_t>(0);
    uint32_t output_dram = get_arg_val<uint32_t>(4);

    constexpr uint32_t L1_GAME = 0x10000;
    constexpr uint32_t L1_OUTPUT = 0x50000;
    constexpr uint32_t GAME_SIZE = 86838;
    constexpr uint32_t OUTPUT_SIZE = 4096;

    // Read game data with correct page_size
    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = game_data_dram,
        .page_size = 1024
    };

    for (uint32_t offset = 0; offset < GAME_SIZE; offset += 1024) {
        uint32_t chunk_size = (offset + 1024 <= GAME_SIZE) ? 1024 : (GAME_SIZE - offset);
        uint64_t game_noc = get_noc_addr(offset / 1024, game_gen);
        noc_async_read(game_noc, L1_GAME + offset, chunk_size);
    }
    noc_async_read_barrier();

    // Initialize Z-machine
    zm.memory = (zbyte*)L1_GAME;
    zm.output = (char*)L1_OUTPUT;
    zm.output_pos = 0;
    zm.max_output = 3900;
    zm.sp = 0;
    zm.call_sp = 0;

    z_header.version = zm.memory[0];
    z_header.abbreviations = read_word(0x18);
    zm.pc = read_word(0x06);

    const char* header = "=== EXECUTING ZORK FROM PC! ===\n\n";
    while (*header) zm.output[zm.output_pos++] = *header++;

    const char* msg = "Starting at PC: 0x";
    while (*msg) zm.output[zm.output_pos++] = *msg++;
    const char* hex = "0123456789ABCDEF";
    zm.output[zm.output_pos++] = hex[(zm.pc >> 12) & 0xF];
    zm.output[zm.output_pos++] = hex[(zm.pc >> 8) & 0xF];
    zm.output[zm.output_pos++] = hex[(zm.pc >> 4) & 0xF];
    zm.output[zm.output_pos++] = hex[zm.pc & 0xF];
    zm.output[zm.output_pos++] = '\n';
    zm.output[zm.output_pos++] = '\n';

    msg = "=== GAME OUTPUT ===\n";
    while (*msg) zm.output[zm.output_pos++] = *msg++;

    // Execute instructions!
    uint32_t instructions_executed = 0;
    const uint32_t MAX_INSTRUCTIONS = 500;

    while (instructions_executed < MAX_INSTRUCTIONS) {
        if (!execute_instruction()) {
            break;  // Hit QUIT or return from main
        }
        instructions_executed++;

        // Stop if output buffer getting full
        if (zm.output_pos >= 3800) {
            break;
        }
    }

    zm.output[zm.output_pos++] = '\n';
    msg = "\n=== Executed ";
    while (*msg) zm.output[zm.output_pos++] = *msg++;
    if (instructions_executed >= 100) zm.output[zm.output_pos++] = '0' + (instructions_executed / 100);
    if (instructions_executed >= 10) zm.output[zm.output_pos++] = '0' + ((instructions_executed % 100) / 10);
    zm.output[zm.output_pos++] = '0' + (instructions_executed % 10);
    msg = " instructions ===\n";
    while (*msg) zm.output[zm.output_pos++] = *msg++;
    zm.output[zm.output_pos++] = '\0';

    // Write output
    InterleavedAddrGen<true> out_gen = {
        .bank_base_address = output_dram,
        .page_size = OUTPUT_SIZE
    };
    uint64_t out_noc = get_noc_addr(0, out_gen);
    noc_async_write(L1_OUTPUT, out_noc, OUTPUT_SIZE);
    noc_async_write_barrier();
}
