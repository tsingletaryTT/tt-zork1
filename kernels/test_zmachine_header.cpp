// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * test_zmachine_header.cpp - Parse complete Z-machine header
 *
 * Z-machine Version 3 header format (first 64 bytes):
 * 0x00: Version (should be 3)
 * 0x01: Flags
 * 0x04-05: High memory base address
 * 0x06-07: Initial PC (program counter) - WHERE CODE STARTS!
 * 0x08-09: Dictionary location
 * 0x0A-0B: Object table location
 * 0x0C-0D: Global variables table
 * 0x0E-0F: Static memory base
 * 0x18-19: Abbreviations table
 * 0x1A-1B: File length
 * 0x1C-1D: Checksum
 */

void kernel_main() {
    // Get runtime arguments
    uint32_t game_data_dram = get_arg_val<uint32_t>(0);
    uint32_t output_dram = get_arg_val<uint32_t>(4);

    // L1 addresses
    constexpr uint32_t L1_GAME_BUFFER = 0x10000;
    constexpr uint32_t L1_OUTPUT_BUFFER = 0x20000;
    constexpr uint32_t HEADER_SIZE = 64;  // Z-machine header is 64 bytes
    constexpr uint32_t OUTPUT_SIZE = 512;

    // Step 1: Read header DRAM → L1
    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = game_data_dram,
        .page_size = HEADER_SIZE
    };
    uint64_t game_noc_addr = get_noc_addr(0, game_gen);
    noc_async_read(game_noc_addr, L1_GAME_BUFFER, HEADER_SIZE);
    noc_async_read_barrier();

    // Step 2: Parse header in L1
    uint8_t* header = (uint8_t*)L1_GAME_BUFFER;
    char* output = (char*)L1_OUTPUT_BUFFER;
    uint32_t pos = 0;

    // Helper: read 16-bit big-endian value
    auto read_word = [&](uint32_t offset) -> uint16_t {
        return (header[offset] << 8) | header[offset + 1];
    };

    // Helper: convert nibble to hex
    auto nibble_to_hex = [](uint8_t n) -> char {
        return (n < 10) ? ('0' + n) : ('A' + n - 10);
    };

    // Helper: write hex word (4 hex digits)
    auto write_hex_word = [&](uint16_t value) {
        output[pos++] = '0';
        output[pos++] = 'x';
        output[pos++] = nibble_to_hex((value >> 12) & 0xF);
        output[pos++] = nibble_to_hex((value >> 8) & 0xF);
        output[pos++] = nibble_to_hex((value >> 4) & 0xF);
        output[pos++] = nibble_to_hex(value & 0xF);
    };

    // Helper: write string
    auto write_str = [&](const char* str) {
        while (*str != '\0') {
            output[pos++] = *str++;
        }
    };

    // Parse key fields
    uint8_t version = header[0x00];
    uint16_t high_mem = read_word(0x04);
    uint16_t initial_pc = read_word(0x06);
    uint16_t dictionary = read_word(0x08);
    uint16_t objects = read_word(0x0A);
    uint16_t globals = read_word(0x0C);
    uint16_t static_mem = read_word(0x0E);
    uint16_t abbrev = read_word(0x18);

    // Format output
    write_str("=== Z-MACHINE HEADER PARSED ===\n\n");

    write_str("Version: ");
    output[pos++] = '0' + version;
    write_str("\n\n");

    write_str("Initial PC: ");
    write_hex_word(initial_pc);
    write_str(" <- CODE STARTS HERE!\n");

    write_str("High memory: ");
    write_hex_word(high_mem);
    write_str("\n");

    write_str("Dictionary: ");
    write_hex_word(dictionary);
    write_str("\n");

    write_str("Objects: ");
    write_hex_word(objects);
    write_str("\n");

    write_str("Globals: ");
    write_hex_word(globals);
    write_str("\n");

    write_str("Static mem: ");
    write_hex_word(static_mem);
    write_str("\n");

    write_str("Abbreviations: ");
    write_hex_word(abbrev);
    write_str("\n\n");

    // Show first instruction location
    write_str("Next step: Read instruction at PC ");
    write_hex_word(initial_pc);
    write_str("\n");

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
