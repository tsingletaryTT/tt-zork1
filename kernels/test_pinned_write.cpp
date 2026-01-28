// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * Test kernel: Write message directly to host memory via PinnedMemory
 *
 * This kernel demonstrates direct RISC-V → host memory writes using NoC.
 * The NoC address points to pinned host memory, not DRAM!
 *
 * Compile-time defines expected:
 * - OUTPUT_NOC_ADDR: 64-bit NoC address of pinned host memory
 * - PCIE_XY_ENC: PCIe XY encoding for the address
 */

#include <cstdint>
#include "api/dataflow/dataflow_api.h"

// L1 buffer for message
constexpr uint32_t L1_BUFFER_ADDR = 0x20000;

void kernel_main() {
    // Message to write to host memory
    const char message[] = "SUCCESS! RISC-V wrote directly to host RAM via PinnedMemory!";
    const uint32_t message_len = sizeof(message);  // Includes null terminator

    // Copy message to L1 buffer
    volatile char* l1_buffer = reinterpret_cast<volatile char*>(L1_BUFFER_ADDR);
    for (uint32_t i = 0; i < message_len; i++) {
        l1_buffer[i] = message[i];
    }

#ifdef OUTPUT_NOC_ADDR
    // Write directly to host memory via NoC!
    // PinnedMemory provides a 64-bit address that routes through PCIe to host
    uint64_t dst_noc_addr = OUTPUT_NOC_ADDR;

    // Try standard async write first - the address should be properly formatted
    noc_async_write(
        L1_BUFFER_ADDR,     // Source: L1 buffer
        dst_noc_addr,       // Destination: Host pinned memory!
        message_len         // Size
    );

    // Wait for write to complete
    noc_async_write_barrier();
#else
    #error "OUTPUT_NOC_ADDR not defined - kernel cannot write to host memory"
#endif
}
