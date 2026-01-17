// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_dict_debug.cpp - Debug dictionary structure parsing
 *
 * Dump raw bytes at dictionary address to understand structure.
 * Z-machine v3 dictionary format per spec:
 * - Byte 0: number of separator characters
 * - Bytes 1..n: separator characters
 * - Byte n+1: entry length
 * - Bytes n+2, n+3: number of entries (word, big-endian)
 * - Following bytes: dictionary entries
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
    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = game_data_dram,
        .page_size = GAME_SIZE
    };
    uint64_t game_noc = get_noc_addr(0, game_gen);
    noc_async_read(game_noc, L1_GAME, GAME_SIZE);
    noc_async_read_barrier();

    zbyte* story_data = (zbyte*)L1_GAME;
    char* output = (char*)L1_OUTPUT;
    uint32_t pos = 0;

    const char* header = "=== ZORK DICTIONARY DEBUG ===\n\n";
    while (*header) output[pos++] = *header++;

    // Read dictionary address from header
    zword dict_addr = (story_data[0x08] << 8) | story_data[0x09];

    const char* msg = "Dictionary at: 0x";
    while (*msg) output[pos++] = *msg++;
    const char* hex = "0123456789ABCDEF";
    output[pos++] = hex[(dict_addr >> 12) & 0xF];
    output[pos++] = hex[(dict_addr >> 8) & 0xF];
    output[pos++] = hex[(dict_addr >> 4) & 0xF];
    output[pos++] = hex[dict_addr & 0xF];
    output[pos++] = '\n';
    output[pos++] = '\n';

    // Dump first 32 bytes of dictionary
    msg = "First 32 bytes:\n";
    while (*msg) output[pos++] = *msg++;

    for (uint32_t i = 0; i < 32; i++) {
        zbyte byte = story_data[dict_addr + i];
        output[pos++] = hex[(byte >> 4) & 0xF];
        output[pos++] = hex[byte & 0xF];
        output[pos++] = ' ';

        if ((i + 1) % 16 == 0) {
            output[pos++] = '\n';
        }
    }
    output[pos++] = '\n';

    // Parse structure step by step
    zbyte num_seps = story_data[dict_addr];
    msg = "Num separators: ";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = '0' + (num_seps / 10);
    output[pos++] = '0' + (num_seps % 10);
    output[pos++] = '\n';

    // Show separator bytes
    msg = "Separators: ";
    while (*msg) output[pos++] = *msg++;
    for (uint32_t i = 0; i < num_seps; i++) {
        zbyte sep = story_data[dict_addr + 1 + i];
        output[pos++] = hex[(sep >> 4) & 0xF];
        output[pos++] = hex[sep & 0xF];
        output[pos++] = ' ';
    }
    output[pos++] = '\n';

    // Entry length
    zword entry_addr = dict_addr + 1 + num_seps;
    zbyte entry_len = story_data[entry_addr];
    msg = "Entry length: ";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = '0' + (entry_len / 10);
    output[pos++] = '0' + (entry_len % 10);
    output[pos++] = '\n';

    // Number of entries (word, big-endian)
    zword num_entries = (story_data[entry_addr + 1] << 8) | story_data[entry_addr + 2];
    msg = "Num entries: ";
    while (*msg) output[pos++] = *msg++;
    if (num_entries >= 1000) output[pos++] = '0' + (num_entries / 1000);
    if (num_entries >= 100) output[pos++] = '0' + ((num_entries % 1000) / 100);
    if (num_entries >= 10) output[pos++] = '0' + ((num_entries % 100) / 10);
    output[pos++] = '0' + (num_entries % 10);
    output[pos++] = '\n';

    // Show bytes at entry count location
    msg = "Entry count bytes [";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = '0' + ((entry_addr + 1) / 10);
    output[pos++] = '0' + ((entry_addr + 1) % 10);
    msg = "]: ";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = hex[(story_data[entry_addr + 1] >> 4) & 0xF];
    output[pos++] = hex[story_data[entry_addr + 1] & 0xF];
    output[pos++] = ' ';
    output[pos++] = hex[(story_data[entry_addr + 2] >> 4) & 0xF];
    output[pos++] = hex[story_data[entry_addr + 2] & 0xF];
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
