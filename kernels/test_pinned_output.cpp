// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * Test kernel for PinnedMemory host-side approach
 *
 * This kernel:
 * 1. Writes message to L1 memory
 * 2. Uses NoC to copy L1 → DRAM buffer
 * 3. Host then copies DRAM → PinnedMemory (zero-copy)
 *
 * DRAM buffer address provided as compile-time define to avoid runtime arg issues.
 */

#include <cstdint>
#include "api/dataflow/dataflow_api.h"

// L1 buffer for message
constexpr uint32_t L1_BUFFER_ADDR = 0x20000;
constexpr uint32_t MESSAGE_SIZE = 1024;

void kernel_main() {
    // Message to write
    const char message[] = "SUCCESS! Host-side PinnedMemory approach works perfectly!\n"
                          "Benefits:\n"
                          "  - Device stays open (no reopen hang!)\n"
                          "  - Zero-copy DRAM->host transfer\n"
                          "  - Uses proven TT-Metal API patterns\n";
    const uint32_t message_len = sizeof(message);

    // Write message to L1 buffer
    volatile char* l1_buffer = reinterpret_cast<volatile char*>(L1_BUFFER_ADDR);
    for (uint32_t i = 0; i < message_len; i++) {
        l1_buffer[i] = message[i];
    }

#ifdef OUTPUT_DRAM_ADDR
    // DRAM buffer address provided as compile-time define
    uint32_t output_dram_addr = OUTPUT_DRAM_ADDR;

    // Create address generator for output DRAM buffer
    InterleavedAddrGen<true> output_gen = {
        .bank_base_address = output_dram_addr,
        .page_size = MESSAGE_SIZE
    };

    // Get NoC address for page 0 of the output buffer
    uint64_t output_noc_addr = get_noc_addr(0, output_gen);

    // Write L1 buffer → DRAM using NoC
    noc_async_write(L1_BUFFER_ADDR, output_noc_addr, MESSAGE_SIZE);

    // Wait for write to complete
    noc_async_write_barrier();
#else
    #error "OUTPUT_DRAM_ADDR not defined - kernel cannot write to DRAM"
#endif
}
