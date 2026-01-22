// Minimal "Hello World" kernel for RISC-V
// Uses NoC to write from L1 to DRAM

#include <cstdint>
#include "api/dataflow/dataflow_api.h"

// OUTPUT_DRAM_ADDR is defined at compile time by host via DataMovementConfig.defines
#ifndef OUTPUT_DRAM_ADDR
#error "OUTPUT_DRAM_ADDR must be defined"
#endif

void kernel_main() {
    // Step 1: Write message to L1 buffer
    constexpr uint32_t l1_addr = 0x20000;  // 128KB into L1
    volatile char* l1_buffer = reinterpret_cast<volatile char*>(l1_addr);

    const char* msg = "HELLO FROM BLACKHOLE RISC-V CORE!\n";
    uint32_t msg_len = 0;
    while (msg[msg_len] != '\0') {
        l1_buffer[msg_len] = msg[msg_len];
        msg_len++;
    }
    l1_buffer[msg_len] = '\0';
    msg_len++;  // Include null terminator

    // Round up to 32-byte alignment for NoC transfer
    uint32_t transfer_size = ((msg_len + 31) / 32) * 32;

    // Step 2: Use NoC to write from L1 to DRAM
    // DRAM is on specific NoC coordinates - need to determine these
    // For now, try common DRAM coordinates (these vary by device)
    uint64_t dram_noc_addr = get_noc_addr(0, 0, OUTPUT_DRAM_ADDR);

    noc_async_write(l1_addr, dram_noc_addr, transfer_size);
    noc_async_write_barrier();

    // Done! Message transferred from L1 to DRAM via NoC
}
