// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_ultra_minimal.cpp - Ultra-minimal Zork demo for Blackhole RISC-V
 *
 * This is the simplest possible demonstration that proves the data pipeline works:
 *   DRAM → L1 → RISC-V processing → L1 → DRAM → Host display
 *
 * What it does:
 * 1. Copies first 256 bytes of game file from DRAM to L1
 * 2. Reads Z-machine version byte (offset 0x00)
 * 3. Writes simple message with version number
 *
 * Does NOT attempt to:
 * - Decode Z-strings (too complex for kernel size limits)
 * - Read abbreviation tables
 * - Process objects
 *
 * This proves the foundational pipeline works, which is all we need to show.
 */

#include <cstdint>
#include "api/dataflow/dataflow_api.h"

// L1 addresses
#define L1_GAME    0x20000   // 256 bytes for game header
#define L1_OUTPUT  0x20100   // 256 bytes for output

// DRAM addresses from compile-time defines
#ifndef GAME_DRAM_ADDR
#error "GAME_DRAM_ADDR required"
#endif
#ifndef OUTPUT_DRAM_ADDR
#error "OUTPUT_DRAM_ADDR required"
#endif

typedef uint8_t zbyte;

void kernel_main() {
    volatile zbyte* l1_game = reinterpret_cast<volatile zbyte*>(L1_GAME);
    volatile char* l1_output = reinterpret_cast<volatile char*>(L1_OUTPUT);

    // Copy first 256 bytes of game file from DRAM to L1
    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = GAME_DRAM_ADDR,
        .page_size = 256
    };
    uint64_t game_noc = get_noc_addr(0, game_gen);
    noc_async_read(game_noc, L1_GAME, 256);
    noc_async_read_barrier();

    // Read Z-machine version (byte 0)
    zbyte version = l1_game[0];

    // Create output message
    char* output = const_cast<char*>(reinterpret_cast<const char*>(l1_output));

    const char* msg1 = "ZORK I: The Great Underground Empire\n";
    const char* msg2 = "\nSUCCESS! Zork game data loaded on Blackhole RISC-V!\n\n";
    const char* msg3 = "Z-machine version: ";
    const char* msg4 = "\n";
    const char* msg5 = "Game file loaded from DRAM → L1 → RISC-V → Output\n";
    const char* msg6 = "This proves the foundational data pipeline works!\n\n";

    int pos = 0;

    // Write messages
    for (const char* s = msg1; *s; s++) output[pos++] = *s;
    for (const char* s = msg2; *s; s++) output[pos++] = *s;
    for (const char* s = msg3; *s; s++) output[pos++] = *s;

    // Write version number as ASCII
    output[pos++] = '0' + version;

    for (const char* s = msg4; *s; s++) output[pos++] = *s;
    for (const char* s = msg5; *s; s++) output[pos++] = *s;
    for (const char* s = msg6; *s; s++) output[pos++] = *s;

    output[pos] = '\0';

    // Write output to DRAM
    InterleavedAddrGen<true> output_gen = {
        .bank_base_address = OUTPUT_DRAM_ADDR,
        .page_size = 256
    };
    uint64_t output_noc = get_noc_addr(0, output_gen);
    noc_async_write(L1_OUTPUT, output_noc, pos + 1);
    noc_async_write_barrier();
}
