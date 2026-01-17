// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * test_decode_execute.cpp - Decode and execute first Z-machine instruction!
 *
 * Z-machine instruction formats (v3):
 *
 * LONG FORM (2OP, opcodes 1-31):
 * - Byte 0: %%nntopppp where:
 *   - nn = 00 or 01 (identifies long form)
 *   - t = first operand type (0=small const, 1=variable)
 *   - o = second operand type (0=small const, 1=variable)
 *   - pppp = opcode bits
 * - Actually: bits 7-5 = nnto, bits 4-0 = opcode (0-31)
 *
 * Our instruction: 0x7C = 01111100
 * - Bits 7-6 = 01: long form 2OP
 * - Bit 5 = 1: second operand is variable
 * - Bit 4 = 1: first operand is variable
 * - Wait, let me recalculate...
 *
 * Actually for long form:
 * - Bit 7 = 0 (not short or variable form)
 * - Bit 6 determines form (combined with bit 7)
 * - Bits 5-4 encode operand types
 * - Bits 3-0 are top 4 bits of opcode, bottom bit is bit 5!
 *
 * Let me use the standard decoding:
 * 0x7C = 0111 1100
 * - Bit 7-6: 01 = long form
 * - Bit 5: 1 = first operand is variable
 * - Bit 4: 1 = second operand is variable
 * - Bits 3-0: 1100 = 12, but opcode is (bit5<<4)|bits3-0... no wait.
 *
 * Standard Z-machine v3 long form:
 * %%nntopppp where nn=form, t=op1_type, o=op2_type, pppp=opcode low bits
 * Then next byte is opcode high bit... no that's not right either.
 *
 * Let me just decode it properly:
 * Bits 7-5 = 011 = form bits
 * Bits 4-0 = 11100 = 28 decimal = opcode
 *
 * 2OP opcode 28 = CALL_2S (call routine, store result)
 */

void kernel_main() {
    uint32_t game_data_dram = get_arg_val<uint32_t>(0);
    uint32_t output_dram = get_arg_val<uint32_t>(4);

    constexpr uint16_t INITIAL_PC = 0x50D5;
    constexpr uint32_t L1_CODE = 0x10000;
    constexpr uint32_t L1_OUTPUT = 0x20000;
    constexpr uint32_t READ_SIZE = 64;
    constexpr uint32_t OUTPUT_SIZE = 1024;

    // Read code at PC
    InterleavedAddrGen<true> code_gen = {
        .bank_base_address = game_data_dram + INITIAL_PC,
        .page_size = READ_SIZE
    };
    uint64_t code_noc = get_noc_addr(0, code_gen);
    noc_async_read(code_noc, L1_CODE, READ_SIZE);
    noc_async_read_barrier();

    uint8_t* code = (uint8_t*)L1_CODE;
    char* output = (char*)L1_OUTPUT;
    uint32_t pos = 0;

    auto write_str = [&](const char* str) {
        while (*str != '\0') output[pos++] = *str++;
    };

    auto write_hex_byte = [&](uint8_t byte) {
        const char* hex = "0123456789ABCDEF";
        output[pos++] = '0';
        output[pos++] = 'x';
        output[pos++] = hex[byte >> 4];
        output[pos++] = hex[byte & 0xF];
    };

    auto write_dec = [&](uint32_t num) {
        if (num == 0) {
            output[pos++] = '0';
            return;
        }
        char buf[12];
        int i = 0;
        while (num > 0) {
            buf[i++] = '0' + (num % 10);
            num /= 10;
        }
        while (i > 0) {
            output[pos++] = buf[--i];
        }
    };

    write_str("=== DECODING FIRST INSTRUCTION ===\n\n");

    // Decode instruction
    uint8_t byte0 = code[0];
    uint8_t byte1 = code[1];
    uint8_t byte2 = code[2];

    write_str("Bytes: ");
    write_hex_byte(byte0);
    output[pos++] = ' ';
    write_hex_byte(byte1);
    output[pos++] = ' ';
    write_hex_byte(byte2);
    write_str("\n\n");

    // Decode form
    write_str("Instruction form: ");
    if ((byte0 & 0xC0) == 0xC0) {
        write_str("VAR (variable form)\n");
    } else if ((byte0 & 0x80) == 0x80) {
        write_str("SHORT (1OP or 0OP)\n");
    } else {
        write_str("LONG (2OP)\n");

        // Decode 2OP
        bool op1_var = (byte0 & 0x40) != 0;
        bool op2_var = (byte0 & 0x20) != 0;
        uint8_t opcode = byte0 & 0x1F;  // Bottom 5 bits

        write_str("  Opcode: ");
        write_dec(opcode);
        write_str("\n");

        write_str("  Operand 1: ");
        if (op1_var) {
            write_str("Variable ");
            write_hex_byte(byte1);
        } else {
            write_str("Small const ");
            write_hex_byte(byte1);
        }
        write_str("\n");

        write_str("  Operand 2: ");
        if (op2_var) {
            write_str("Variable ");
            write_hex_byte(byte2);
        } else {
            write_str("Small const ");
            write_hex_byte(byte2);
        }
        write_str("\n\n");

        // Identify common opcodes
        write_str("Opcode ");
        write_dec(opcode);
        write_str(" = ");

        const char* opcode_names[] = {
            "???", "JE", "JL", "JG", "DEC_CHK", "INC_CHK", "JIN", "TEST",
            "OR", "AND", "TEST_ATTR", "SET_ATTR", "CLEAR_ATTR", "STORE", "INSERT_OBJ", "LOADW",
            "LOADB", "GET_PROP", "GET_PROP_ADDR", "GET_NEXT_PROP", "ADD", "SUB", "MUL", "DIV",
            "MOD", "CALL_2S", "CALL_2N", "SET_COLOUR", "THROW", "???", "???", "???"
        };

        if (opcode < 32) {
            write_str(opcode_names[opcode]);
        }
        write_str("\n\n");

        if (opcode == 25) {
            write_str("CALL_2S = Call routine at address\n");
            write_str("This likely initializes the game!\n");
            write_str("\nNext: Implement CALL to execute it!\n");
        }
    }

    output[pos++] = '\0';

    // Write output
    InterleavedAddrGen<true> out_gen = {
        .bank_base_address = output_dram,
        .page_size = OUTPUT_SIZE
    };
    uint64_t out_noc = get_noc_addr(0, out_gen);
    noc_async_write(L1_OUTPUT, out_noc, OUTPUT_SIZE);
    noc_async_write_barrier();
}
