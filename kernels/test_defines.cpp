// test_defines.cpp — Minimal kernel to verify defines + NoC write in generic_op
//
// Writes "HELLO FROM RISC-V\n" to L1 at 0x20000, then NoC-writes to OUTPUT_DRAM_ADDR.
// If generic_op + defines + NoC write all work, we see this string in the output tensor.

#include <cstdint>
#include "api/dataflow/dataflow_api.h"

#ifndef OUTPUT_DRAM_ADDR
#error "OUTPUT_DRAM_ADDR must be defined"
#endif

void kernel_main() {
    constexpr uint32_t L1_BUF = 0x20000;
    volatile char* buf = reinterpret_cast<volatile char*>(L1_BUF);

    const char* msg = "HELLO FROM RISC-V DEFINES TEST\n";
    int i = 0;
    while (msg[i]) {
        buf[i] = msg[i];
        i++;
    }
    buf[i] = '\0';

    uint64_t dst = get_noc_addr(0, 0, OUTPUT_DRAM_ADDR);
    noc_async_write(L1_BUF, dst, 64);
    noc_async_write_barrier();
}
