// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * test_read_game.cpp - Read game data from DRAM and display as hex
 *
 * This kernel reads the first 16 bytes of zork1.z3 (Z-machine header)
 * and formats them as hex for verification.
 *
 * Expected Z-machine header (first 4 bytes):
 * 0x03 = Version 3
 * 0x?? = Flags
 * 0x?? 0x?? = Release number
 */

void kernel_main() {
    // Get runtime arguments
    uint32_t game_data_dram = get_arg_val<uint32_t>(0);  // Game data in DRAM
    uint32_t game_data_size = get_arg_val<uint32_t>(1);  // Size
    uint32_t output_dram = get_arg_val<uint32_t>(4);     // Output buffer in DRAM

    // L1 addresses
    constexpr uint32_t L1_GAME_BUFFER = 0x10000;  // For game data
    constexpr uint32_t L1_OUTPUT_BUFFER = 0x20000; // For output message
    constexpr uint32_t READ_SIZE = 16;  // Read first 16 bytes of game
    constexpr uint32_t OUTPUT_SIZE = 256; // Plenty of space for formatted output

    // Step 1: Read game data DRAM → L1
    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = game_data_dram,
        .page_size = READ_SIZE
    };
    uint64_t game_noc_addr = get_noc_addr(0, game_gen);
    noc_async_read(game_noc_addr, L1_GAME_BUFFER, READ_SIZE);
    noc_async_read_barrier();  // Wait for read to complete

    // Step 2: Format game data as hex in L1 output buffer
    char* output = (char*)L1_OUTPUT_BUFFER;
    uint8_t* game_bytes = (uint8_t*)L1_GAME_BUFFER;

    // Helper: convert nibble to hex char
    auto nibble_to_hex = [](uint8_t n) -> char {
        return (n < 10) ? ('0' + n) : ('A' + n - 10);
    };

    // Write header
    const char* header = "Z-machine header (first 16 bytes):\n";
    uint32_t pos = 0;
    while (header[pos] != '\0') {
        output[pos] = header[pos];
        pos++;
    }

    // Format each byte as hex
    for (uint32_t i = 0; i < READ_SIZE; i++) {
        uint8_t byte = game_bytes[i];

        // Write "0x"
        output[pos++] = '0';
        output[pos++] = 'x';

        // High nibble
        output[pos++] = nibble_to_hex(byte >> 4);

        // Low nibble
        output[pos++] = nibble_to_hex(byte & 0xF);

        // Space separator
        output[pos++] = ' ';

        // Newline every 4 bytes for readability
        if ((i + 1) % 4 == 0) {
            output[pos++] = '\n';
        }
    }

    // Add interpretation
    const char* footer = "\nVersion: 0x";
    uint32_t j = 0;
    while (footer[j] != '\0') {
        output[pos++] = footer[j++];
    }
    output[pos++] = nibble_to_hex(game_bytes[0] >> 4);
    output[pos++] = nibble_to_hex(game_bytes[0] & 0xF);
    output[pos++] = '\n';
    output[pos++] = '\0';

    // Step 3: Write output L1 → DRAM
    InterleavedAddrGen<true> output_gen = {
        .bank_base_address = output_dram,
        .page_size = OUTPUT_SIZE
    };
    uint64_t output_noc_addr = get_noc_addr(0, output_gen);
    noc_async_write(L1_OUTPUT_BUFFER, output_noc_addr, OUTPUT_SIZE);
    noc_async_write_barrier();  // Wait for write to complete

    // Done! Game header is now in output buffer
}
