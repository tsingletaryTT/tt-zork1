// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * Test kernel: Write message directly to host memory via PinnedMemory
 *
 * This kernel demonstrates direct RISC-V → host memory writes using NoC.
 * The NoC address points to pinned host memory, not DRAM!
 *
 * Compile-time defines expected:
 * - OUTPUT_NOC_ADDR: NoC address of pinned host memory
 * - PCIE_XY_ENC: PCIe XY encoding for the address
 */

#include <cstdint>
#include "api/dataflow/dataflow_api.h"

// L1 buffer for message
constexpr uint32_t L1_BUFFER_ADDR = 0x20000;

void kernel_main() {
    // Message to write to host memory
    const char message[] = "Hello from Blackhole RISC-V core! PinnedMemory works!";
    const uint32_t message_len = sizeof(message);  // Includes null terminator

    // Copy message to L1 buffer
    volatile char* l1_buffer = reinterpret_cast<volatile char*>(L1_BUFFER_ADDR);
    for (uint32_t i = 0; i < message_len; i++) {
        l1_buffer[i] = message[i];
    }

#ifdef OUTPUT_NOC_ADDR
    // Write directly to host memory via NoC!
    // The OUTPUT_NOC_ADDR is provided by the host and points to pinned host memory
    noc_async_write(
        L1_BUFFER_ADDR,          // Source: L1 buffer
        OUTPUT_NOC_ADDR,         // Destination: Host pinned memory!
        message_len,             // Size
        0                        // NOC 0
    );

    // Wait for write to complete
    noc_async_write_barrier();
#else
    #error "OUTPUT_NOC_ADDR not defined - kernel cannot write to host memory"
#endif
}
