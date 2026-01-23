// Simple kernel that reads a counter from state, increments it, writes back
#include <cstdint>
#include "dataflow_api.h"

void kernel_main() {
    // Read state from DRAM to L1
    constexpr uint32_t L1_STATE = 0x20000;
    constexpr uint32_t STATE_SIZE = 16 * 1024;

    uint64_t state_dram_noc_addr = get_noc_addr(0, 0, STATE_DRAM_ADDR);
    noc_async_read(state_dram_noc_addr, L1_STATE, STATE_SIZE);
    noc_async_read_barrier();

    // Read counter (first 4 bytes)
    volatile uint32_t* state = reinterpret_cast<volatile uint32_t*>(L1_STATE);
    uint32_t counter = state[0];

    // Increment it
    counter += 10;  // Add 10 each time kernel runs

    // Write back
    state[0] = counter;

    // Write to DRAM
    noc_async_write(L1_STATE, state_dram_noc_addr, STATE_SIZE);
    noc_async_write_barrier();
}
