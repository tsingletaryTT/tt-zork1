// test_noc_read.cpp — Diagnostic kernel: test DRAM→L1 reads without interpreter.
//
// Reads game data from DRAM into L1 in one batched pass (identical to the main
// zork_interpreter_l1.cpp NOC read sequence), then writes the first 64 bytes of
// the game data back to the output DRAM buffer. If this completes, NOC reads of
// 87 KB game data work. If it hangs, the NOC read (not the interpreter) is the
// problem.
//
// Defines required (passed via KernelDescriptor.defines):
//   GAME_DRAM_ADDR   — DRAM address of game data tensor
//   OUTPUT_DRAM_ADDR — DRAM address of output buffer tensor
//
// Expected output on success:
//   Bytes 0-7 are the Zork V3 Z-machine header:
//     [0] = 0x03  (version 3)
//     [2] = 0x00  (flags)
//     [4][5] = high/low bytes of release number
//   "NOC_READ_OK" string written after the header bytes.

#include <cstdint>
#include "api/dataflow/dataflow_api.h"

#ifndef GAME_DRAM_ADDR
#error "GAME_DRAM_ADDR must be defined"
#endif
#ifndef OUTPUT_DRAM_ADDR
#error "OUTPUT_DRAM_ADDR must be defined"
#endif

void kernel_main() {
    // L1 layout for this test — minimal, no overlap with anything.
    constexpr uint32_t L1_GAME = 0x10000;  // 87040 B game data
    constexpr uint32_t L1_OUT  = 0x30000;  // output buffer
    constexpr uint32_t GAME_SIZE  = 87040;
    constexpr uint32_t CHUNK_SIZE = 4096;

    // Batch all game DRAM→L1 reads (same as main kernel), single barrier.
    uint32_t offset = 0;
    while (offset < GAME_SIZE) {
        uint32_t chunk = (offset + CHUNK_SIZE > GAME_SIZE) ? (GAME_SIZE - offset) : CHUNK_SIZE;
        uint64_t src = get_noc_addr(0, 0, GAME_DRAM_ADDR + offset);
        noc_async_read(src, L1_GAME + offset, chunk);
        offset += chunk;
    }
    noc_async_read_barrier();  // Wait for all 22 game chunks

    // At this point L1[0x10000..0x15400) holds the raw Zork Z-machine file.
    // Write a confirmation message + first game bytes to L1_OUT, then NOC-write to DRAM.
    volatile uint8_t* buf = reinterpret_cast<volatile uint8_t*>(L1_OUT);
    volatile uint8_t* game = reinterpret_cast<volatile uint8_t*>(L1_GAME);

    // Write confirmation tag
    const char* tag = "NOC_READ_OK header=";
    uint32_t i = 0;
    while (tag[i]) { buf[i] = tag[i]; i++; }

    // Write first 8 game bytes as hex for inspection
    const char* hex = "0123456789ABCDEF";
    for (uint32_t b = 0; b < 8; b++) {
        uint8_t byte = game[b];
        buf[i++] = hex[(byte >> 4) & 0xF];
        buf[i++] = hex[byte & 0xF];
        buf[i++] = ' ';
    }
    buf[i++] = '\n';
    buf[i] = '\0';

    uint32_t write_len = ((i + 1 + 31) / 32) * 32;  // Round up to 32-byte NOC alignment
    uint64_t dst = get_noc_addr(0, 0, (uint32_t)OUTPUT_DRAM_ADDR);
    noc_async_write(L1_OUT, dst, write_len);
    noc_async_write_barrier();
}
