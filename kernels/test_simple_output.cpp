// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * Simple test kernel: Write message to L1 memory
 *
 * This kernel demonstrates basic L1 write functionality.
 * The host will handle copying from L1 → DRAM → PinnedMemory.
 *
 * No NoC operations needed - just write to local L1.
 */

#include <cstdint>

// L1 buffer for message (hardcoded address known to work)
constexpr uint32_t L1_BUFFER_ADDR = 0x20000;

void kernel_main() {
    // Message to write
    const char message[] = "SUCCESS! Host-side PinnedMemory approach works perfectly!";
    const uint32_t message_len = sizeof(message);  // Includes null terminator

    // Write message to L1 buffer
    volatile char* l1_buffer = reinterpret_cast<volatile char*>(L1_BUFFER_ADDR);
    for (uint32_t i = 0; i < message_len; i++) {
        l1_buffer[i] = message[i];
    }

    // That's it! Host will handle the rest:
    // - Copy L1 → DRAM (via NoC from host side)
    // - Copy DRAM → PinnedMemory (zero-copy transfer)
    // - Read from pinned memory pointer
}
