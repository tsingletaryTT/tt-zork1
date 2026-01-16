// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_kernel.cpp - Zork Z-machine kernel for Blackhole RISC-V cores
 *
 * This is the kernel entry point that runs on Tenstorrent Blackhole RISC-V cores.
 * It receives DRAM buffer pointers from the host and runs the Z-machine interpreter.
 *
 * PROOF OF CONCEPT - Minimal implementation to prove Zork can run on Blackhole.
 *
 * Runtime Arguments (passed via SetRuntimeArgs):
 * arg[0] - Game data buffer address (DRAM)
 * arg[1] - Game data size (bytes)
 * arg[2] - Input buffer address (DRAM)
 * arg[3] - Input buffer size (bytes)
 * arg[4] - Output buffer address (DRAM)
 * arg[5] - Output buffer size (bytes)
 */

#include <cstdint>

// TT-Metal kernel API
// Note: We'll need to include the actual header path when building
// For now, this documents what we need

/**
 * Get runtime argument value
 * This is how the kernel receives buffer pointers from the host
 */
template<typename T>
inline T get_arg_val(uint32_t arg_index) {
    // This will be provided by TT-Metal's dataflow_api.h
    // Placeholder for compilation
    extern uint32_t __kernel_args[];
    return (T)__kernel_args[arg_index];
}

// Forward declarations for Frotz functions we'll call
// These are defined in the Frotz source files we'll link
extern "C" {
    // Main Z-machine interpreter loop
    // Defined in src/zmachine/frotz/src/common/main.c
    int frotz_main(int argc, char *argv[]);

    // Blackhole I/O initialization
    // Defined in src/zmachine/blackhole_io.c
    void blackhole_io_init(
        uint32_t game_data_addr,
        uint32_t game_data_size,
        uint32_t input_addr,
        uint32_t input_size,
        uint32_t output_addr,
        uint32_t output_size
    );
}

/**
 * Kernel entry point
 *
 * This function is called by TT-Metal when the kernel is executed.
 * It receives buffer addresses via runtime arguments, initializes
 * the Blackhole I/O layer, and runs the Z-machine interpreter.
 */
void kernel_main() {
    // Get DRAM buffer addresses from runtime arguments
    uint32_t game_data_addr = get_arg_val<uint32_t>(0);
    uint32_t game_data_size = get_arg_val<uint32_t>(1);
    uint32_t input_addr     = get_arg_val<uint32_t>(2);
    uint32_t input_size     = get_arg_val<uint32_t>(3);
    uint32_t output_addr    = get_arg_val<uint32_t>(4);
    uint32_t output_size    = get_arg_val<uint32_t>(5);

    // Initialize Blackhole I/O layer with DRAM buffer pointers
    blackhole_io_init(
        game_data_addr,
        game_data_size,
        input_addr,
        input_size,
        output_addr,
        output_size
    );

    // Prepare arguments for Frotz main
    // Note: In BUILD_BLACKHOLE mode, Frotz won't actually use argv[1]
    // as the filename, but we pass it for consistency
    const char* game_name = "zork1.z3";
    char* argv[2];
    argv[0] = (char*)"zork";
    argv[1] = (char*)game_name;

    // Run the Z-machine interpreter!
    // This is the main Frotz loop that:
    // 1. Reads game data from DRAM buffer (via blackhole_read_game_data)
    // 2. Processes Z-machine instructions
    // 3. Reads input from DRAM buffer (via blackhole_read_line)
    // 4. Writes output to DRAM buffer (via blackhole_write_string)
    frotz_main(2, argv);

    // Kernel exits, returning control to host
    // Host can now read the output buffer and display results
}
