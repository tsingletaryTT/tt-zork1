// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_verify_load.cpp - Verify game data is loading correctly
 *
 * Check if game data is actually present in L1 memory.
 * Dump header bytes and verify non-zero data.
 */

#include <cstdint>

typedef uint8_t zbyte;
typedef uint16_t zword;

void kernel_main() {
    uint32_t game_data_dram = get_arg_val<uint32_t>(0);
    uint32_t output_dram = get_arg_val<uint32_t>(4);

    constexpr uint32_t L1_GAME = 0x10000;
    constexpr uint32_t L1_OUTPUT = 0x50000;
    constexpr uint32_t GAME_SIZE = 86838;
    constexpr uint32_t OUTPUT_SIZE = 4096;

    // Read game into L1
    // CRITICAL: page_size must match the buffer config (1024 bytes)!
    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = game_data_dram,
        .page_size = 1024  // Must match host buffer page_size!
    };

    // Read the full game data in chunks
    for (uint32_t offset = 0; offset < GAME_SIZE; offset += 1024) {
        uint32_t chunk_size = (offset + 1024 <= GAME_SIZE) ? 1024 : (GAME_SIZE - offset);
        uint64_t game_noc = get_noc_addr(offset / 1024, game_gen);
        noc_async_read(game_noc, L1_GAME + offset, chunk_size);
    }
    noc_async_read_barrier();

    zbyte* story_data = (zbyte*)L1_GAME;
    char* output = (char*)L1_OUTPUT;
    uint32_t pos = 0;

    const char* header = "=== GAME DATA VERIFICATION ===\n\n";
    while (*header) output[pos++] = *header++;

    // Dump first 64 bytes of game data (header)
    const char* msg = "First 64 bytes of game (header):\n";
    while (*msg) output[pos++] = *msg++;

    const char* hex = "0123456789ABCDEF";
    for (uint32_t i = 0; i < 64; i++) {
        zbyte byte = story_data[i];
        output[pos++] = hex[(byte >> 4) & 0xF];
        output[pos++] = hex[byte & 0xF];
        output[pos++] = ' ';

        if ((i + 1) % 16 == 0) {
            output[pos++] = '\n';
        }
    }
    output[pos++] = '\n';

    // Check key header fields
    zbyte version = story_data[0];
    msg = "Version (byte 0): ";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = '0' + version;
    output[pos++] = '\n';

    zword high_mem = (story_data[0x04] << 8) | story_data[0x05];
    msg = "High memory (0x04-05): 0x";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = hex[(high_mem >> 12) & 0xF];
    output[pos++] = hex[(high_mem >> 8) & 0xF];
    output[pos++] = hex[(high_mem >> 4) & 0xF];
    output[pos++] = hex[high_mem & 0xF];
    output[pos++] = '\n';

    zword pc = (story_data[0x06] << 8) | story_data[0x07];
    msg = "PC (0x06-07): 0x";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = hex[(pc >> 12) & 0xF];
    output[pos++] = hex[(pc >> 8) & 0xF];
    output[pos++] = hex[(pc >> 4) & 0xF];
    output[pos++] = hex[pc & 0xF];
    output[pos++] = '\n';

    zword dict = (story_data[0x08] << 8) | story_data[0x09];
    msg = "Dictionary (0x08-09): 0x";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = hex[(dict >> 12) & 0xF];
    output[pos++] = hex[(dict >> 8) & 0xF];
    output[pos++] = hex[(dict >> 4) & 0xF];
    output[pos++] = hex[dict & 0xF];
    output[pos++] = '\n';
    output[pos++] = '\n';

    // Now verify the dictionary address has data
    msg = "Bytes at dictionary (0x";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = hex[(dict >> 12) & 0xF];
    output[pos++] = hex[(dict >> 8) & 0xF];
    output[pos++] = hex[(dict >> 4) & 0xF];
    output[pos++] = hex[dict & 0xF];
    msg = "):\n";
    while (*msg) output[pos++] = *msg++;

    for (uint32_t i = 0; i < 32; i++) {
        zbyte byte = story_data[dict + i];
        output[pos++] = hex[(byte >> 4) & 0xF];
        output[pos++] = hex[byte & 0xF];
        output[pos++] = ' ';

        if ((i + 1) % 16 == 0) {
            output[pos++] = '\n';
        }
    }
    output[pos++] = '\n';

    // Check a few random addresses too
    msg = "Bytes at 0x1000:\n";
    while (*msg) output[pos++] = *msg++;
    for (uint32_t i = 0; i < 16; i++) {
        zbyte byte = story_data[0x1000 + i];
        output[pos++] = hex[(byte >> 4) & 0xF];
        output[pos++] = hex[byte & 0xF];
        output[pos++] = ' ';
    }
    output[pos++] = '\n';
    output[pos++] = '\n';

    msg = "Bytes at 0x5000:\n";
    while (*msg) output[pos++] = *msg++;
    for (uint32_t i = 0; i < 16; i++) {
        zbyte byte = story_data[0x5000 + i];
        output[pos++] = hex[(byte >> 4) & 0xF];
        output[pos++] = hex[byte & 0xF];
        output[pos++] = ' ';
    }
    output[pos++] = '\n';

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
