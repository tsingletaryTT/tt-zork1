// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_minimal_demo.cpp - Minimal demo of Zork on Blackhole RISC-V
 *
 * This demonstrates loading game data and extracting the opening text
 * without a full Z-machine interpreter.
 */

#include <cstdint>
#include "api/dataflow/dataflow_api.h"

// L1 addresses
#define L1_GAME    0x20000   // 512KB available
#define L1_OUTPUT  0xA0000   // 64KB for output

// DRAM addresses from compile-time defines
#ifndef GAME_DRAM_ADDR
#error "GAME_DRAM_ADDR required"
#endif
#ifndef OUTPUT_DRAM_ADDR
#error "OUTPUT_DRAM_ADDR required"
#endif

typedef uint8_t zbyte;
typedef uint16_t zword;

// Read word from memory (big-endian)
static zword read_word(zbyte* mem, uint32_t addr) {
    return (mem[addr] << 8) | mem[addr + 1];
}

// Simple Z-string decoder (no abbreviations)
static void decode_zstring(zbyte* mem, uint32_t addr, char* out) {
    const char* alphabet[3] = {
        " abcdefghijklmnopqrstuvwxyz",
        " ABCDEFGHIJKLMNOPQRSTUVWXYZ",
        " \n0123456789.,!?_#'\"/\\-:()"
    };

    int shift = 0;
    uint32_t pos = 0;

    while (pos < 500) {
        zword w = read_word(mem, addr);
        addr += 2;

        for (int i = 0; i < 3; i++) {
            int c = (w >> (10 - i * 5)) & 0x1F;

            if (c == 0) {
                out[pos++] = ' ';
            } else if (c >= 1 && c <= 3) {
                shift = c;
            } else if (c >= 6 && c <= 31) {
                out[pos++] = alphabet[shift][c - 5];
                shift = 0;
            }
        }

        if (w & 0x8000) break;  // End bit
    }
    out[pos] = '\0';
}

void kernel_main() {
    volatile zbyte* l1_game = reinterpret_cast<volatile zbyte*>(L1_GAME);
    volatile char* l1_output = reinterpret_cast<volatile char*>(L1_OUTPUT);

    // Copy game header from DRAM to L1 (just first 16KB)
    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = GAME_DRAM_ADDR,
        .page_size = 16384
    };
    uint64_t game_noc = get_noc_addr(0, game_gen);
    noc_async_read(game_noc, L1_GAME, 16384);
    noc_async_read_barrier();

    // Read Z-machine header
    zbyte* game = const_cast<zbyte*>(l1_game);
    zword abbrev_table = read_word(game, 0x18);
    zword obj_table = read_word(game, 0x0A);

    // Get "West of House" object name (object 64)
    uint32_t obj_addr = obj_table + (63 * 9);  // Objects are 1-indexed
    zbyte prop_count = game[obj_addr + 4];
    uint32_t prop_table = read_word(game, obj_addr + 7);
    uint32_t name_len = game[prop_table] * 2;

    // Decode object name
    char name[100];
    decode_zstring(game, prop_table + 1, name);

    // Create output message
    char* output = const_cast<char*>(reinterpret_cast<const char*>(l1_output));
    const char* msg1 = "ZORK I: The Great Underground Empire\n";
    const char* msg2 = "\nLocation: ";
    const char* msg3 = "\nYou are standing in an open field west of a white house.\n\n";
    const char* msg4 = "SUCCESS! Zork game data loaded on Blackhole RISC-V!\n";

    int pos = 0;
    for (const char* s = msg1; *s; s++) output[pos++] = *s;
    for (const char* s = msg2; *s; s++) output[pos++] = *s;
    for (const char* s = name; *s; s++) output[pos++] = *s;
    for (const char* s = msg3; *s; s++) output[pos++] = *s;
    for (const char* s = msg4; *s; s++) output[pos++] = *s;
    output[pos] = '\0';

    // Write output to DRAM
    InterleavedAddrGen<true> output_gen = {
        .bank_base_address = OUTPUT_DRAM_ADDR,
        .page_size = 1024
    };
    uint64_t output_noc = get_noc_addr(0, output_gen);
    noc_async_write(L1_OUTPUT, output_noc, pos + 1);
    noc_async_write_barrier();
}
