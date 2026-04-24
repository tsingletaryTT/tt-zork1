// test_hello.cpp — Absolute minimal kernel: write "HELLO" to L1, NoC to DRAM.
#include <cstdint>
#include "api/dataflow/dataflow_api.h"

#ifndef OUTPUT_DRAM_ADDR
#error "OUTPUT_DRAM_ADDR must be defined"
#endif

void kernel_main() {
    constexpr uint32_t L1_BUF = 0x20000;

    volatile uint8_t* buf = reinterpret_cast<volatile uint8_t*>(L1_BUF);
    const char* msg = "HELLO RISC-V DEFINES WORK!\n";
    uint32_t i = 0;
    while (msg[i]) { buf[i] = msg[i]; i++; }
    buf[i] = 0;
    uint32_t len = ((i + 1 + 31) / 32) * 32;

    uint64_t dst = get_noc_addr(0, 0, (uint32_t)OUTPUT_DRAM_ADDR);
    noc_async_write(L1_BUF, dst, len);
    noc_async_write_barrier();
}
