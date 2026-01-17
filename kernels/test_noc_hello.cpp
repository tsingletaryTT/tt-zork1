// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * test_noc_hello.cpp - Write "HELLO RISC-V!" using NoC API
 *
 * Pattern learned from add_2_integers_in_riscv:
 * 1. Get DRAM buffer address from runtime args
 * 2. Create InterleavedAddrGen for the DRAM buffer
 * 3. Write message to L1 memory
 * 4. Use noc_async_write to copy L1 → DRAM
 * 5. Wait for write barrier
 */

void kernel_main() {
    // Get output DRAM buffer address from runtime args
    uint32_t output_dram_addr = get_arg_val<uint32_t>(4);  // arg[4] = output buffer

    // Use a fixed L1 address for our message buffer
    // L1 starts at 0x0, we'll use 0x10000 (64KB offset, safe area)
    constexpr uint32_t L1_BUFFER_ADDR = 0x10000;
    constexpr uint32_t MESSAGE_SIZE = 32;  // Buffer size

    // Write message to L1 memory (direct pointer access is OK in L1!)
    char* l1_message = (char*)L1_BUFFER_ADDR;

    // Write "HELLO RISC-V!" character by character
    l1_message[0] = 'H';
    l1_message[1] = 'E';
    l1_message[2] = 'L';
    l1_message[3] = 'L';
    l1_message[4] = 'O';
    l1_message[5] = ' ';
    l1_message[6] = 'R';
    l1_message[7] = 'I';
    l1_message[8] = 'S';
    l1_message[9] = 'C';
    l1_message[10] = '-';
    l1_message[11] = 'V';
    l1_message[12] = '!';
    l1_message[13] = '\n';
    l1_message[14] = '\0';

    // Create address generator for output DRAM buffer
    // Using page_size = MESSAGE_SIZE to transfer our message
    InterleavedAddrGen<true> output_gen = {
        .bank_base_address = output_dram_addr,
        .page_size = MESSAGE_SIZE
    };

    // Get NoC address for page 0 of the output buffer
    uint64_t output_noc_addr = get_noc_addr(0, output_gen);

    // Write L1 buffer → DRAM using NoC
    // This is NON-BLOCKING, so we need a barrier after
    noc_async_write(L1_BUFFER_ADDR, output_noc_addr, MESSAGE_SIZE);

    // Wait for write to complete before kernel returns
    noc_async_write_barrier();

    // Done! Message is now in DRAM, host can read it
}
