// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * test_first_instruction.cpp - Read and decode first Z-machine instruction
 *
 * We know from the header that initial PC = 0x50D5
 * Let's read the bytes at that address and try to decode the opcode!
 *
 * Z-machine instruction format (Version 3):
 * - First byte contains opcode and sometimes operand types
 * - Opcodes can be 0OP, 1OP, 2OP, or VAR
 * - Common opcodes:
 *   - 0xB2 = print_ret (print string and return true)
 *   - 0xB3 = print (print string)
 *   - Many more...
 */

void kernel_main() {
    // Get runtime arguments
    uint32_t game_data_dram = get_arg_val<uint32_t>(0);
    uint32_t output_dram = get_arg_val<uint32_t>(4);

    // Initial PC from header (we just discovered this!)
    constexpr uint16_t INITIAL_PC = 0x50D5;

    // L1 addresses
    constexpr uint32_t L1_GAME_BUFFER = 0x10000;
    constexpr uint32_t L1_OUTPUT_BUFFER = 0x20000;
    constexpr uint32_t READ_SIZE = 32;  // Read 32 bytes starting at PC
    constexpr uint32_t OUTPUT_SIZE = 512;

    // Helper: read data from game file at specific offset
    auto read_game_at = [&](uint32_t offset, uint32_t dest_l1, uint32_t size) {
        // Create address generator starting at offset
        InterleavedAddrGen<true> game_gen = {
            .bank_base_address = game_data_dram + offset,
            .page_size = size
        };
        uint64_t noc_addr = get_noc_addr(0, game_gen);
        noc_async_read(noc_addr, dest_l1, size);
        noc_async_read_barrier();
    };

    // Step 1: Read bytes at initial PC
    read_game_at(INITIAL_PC, L1_GAME_BUFFER, READ_SIZE);

    // Step 2: Decode in L1
    uint8_t* code = (uint8_t*)L1_GAME_BUFFER;
    char* output = (char*)L1_OUTPUT_BUFFER;
    uint32_t pos = 0;

    // Helper: nibble to hex
    auto nibble_to_hex = [](uint8_t n) -> char {
        return (n < 10) ? ('0' + n) : ('A' + n - 10);
    };

    // Helper: write string
    auto write_str = [&](const char* str) {
        while (*str != '\0') {
            output[pos++] = *str++;
        }
    };

    // Helper: write hex byte
    auto write_hex_byte = [&](uint8_t byte) {
        output[pos++] = '0';
        output[pos++] = 'x';
        output[pos++] = nibble_to_hex(byte >> 4);
        output[pos++] = nibble_to_hex(byte & 0xF);
    };

    // Format output
    write_str("=== FIRST Z-MACHINE INSTRUCTION ===\n\n");
    write_str("Address: 0x50D5\n");
    write_str("Bytecode (first 16 bytes):\n");

    // Show first 16 bytes in hex
    for (uint32_t i = 0; i < 16; i++) {
        write_hex_byte(code[i]);
        output[pos++] = ' ';
        if ((i + 1) % 8 == 0) {
            output[pos++] = '\n';
        }
    }

    output[pos++] = '\n';

    // Decode first byte (opcode)
    uint8_t first_byte = code[0];

    write_str("First byte: ");
    write_hex_byte(first_byte);
    write_str("\n\n");

    // Basic opcode decode (simplified)
    // In Z-machine, opcodes are categorized by top bits:
    // - 0x00-0x1F: 2OP with small constants
    // - 0x20-0x3F: 2OP with small const + var
    // - ...
    // - 0xB0-0xBF: 0OP (no operands)
    // - 0xE0-0xFF: VAR (variable # of operands)

    write_str("Opcode type: ");
    if (first_byte >= 0xB0 && first_byte <= 0xBF) {
        write_str("0OP (no operands)\n");

        uint8_t opcode_num = first_byte & 0x0F;
        write_str("Opcode number: ");
        output[pos++] = '0' + (opcode_num / 10);
        output[pos++] = '0' + (opcode_num % 10);
        write_str("\n\n");

        // Common 0OP opcodes:
        // 0xB0 = rtrue (return true)
        // 0xB1 = rfalse (return false)
        // 0xB2 = print (print literal string)
        // 0xB3 = print_ret (print string and return true)
        // 0xB8 = ret_popped (return value from stack)
        // 0xBB = new_line

        if (opcode_num == 2) {
            write_str("Likely: PRINT (print literal string)\n");
            write_str("Next bytes are Z-string data!\n");
        } else if (opcode_num == 3) {
            write_str("Likely: PRINT_RET (print & return)\n");
            write_str("Next bytes are Z-string data!\n");
        }
    } else if (first_byte >= 0xE0) {
        write_str("VAR (variable operands)\n");
    } else if ((first_byte & 0xC0) == 0x80) {
        write_str("1OP (one operand)\n");
    } else {
        write_str("2OP (two operands)\n");
    }

    output[pos++] = '\n';
    write_str("Next: Implement full decoder!\n");
    output[pos++] = '\0';

    // Step 3: Write output L1 → DRAM
    InterleavedAddrGen<true> output_gen = {
        .bank_base_address = output_dram,
        .page_size = OUTPUT_SIZE
    };
    uint64_t output_noc_addr = get_noc_addr(0, output_gen);
    noc_async_write(L1_OUTPUT_BUFFER, output_noc_addr, OUTPUT_SIZE);
    noc_async_write_barrier();
}
