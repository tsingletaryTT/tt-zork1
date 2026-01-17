// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_monolithic.cpp - Monolithic Zork kernel for Blackhole RISC-V
 *
 * PROOF OF CONCEPT - Single file that includes all Zork sources
 *
 * This is a hack to get around TT-Metal's single-file kernel model.
 * We'll clean it up after proving it works!
 */

// TT-Metal kernel API
#include "api/dataflow/dataflow_api.h"

// Enable Blackhole mode
#define BUILD_BLACKHOLE

// Blackhole I/O layer (our custom DRAM buffer abstraction)
#include "../src/zmachine/blackhole_io.h"
#include "../src/zmachine/blackhole_io.c"

// Now include all Frotz source files
// This is ugly but works for proof of concept!

// Frotz common files
#include "../src/zmachine/frotz/src/common/buffer.c"
#include "../src/zmachine/frotz/src/common/err.c"
#include "../src/zmachine/frotz/src/common/fastmem.c"
#include "../src/zmachine/frotz/src/common/files.c"
#include "../src/zmachine/frotz/src/common/input.c"
#include "../src/zmachine/frotz/src/common/main.c"
#include "../src/zmachine/frotz/src/common/math.c"
#include "../src/zmachine/frotz/src/common/object.c"
#include "../src/zmachine/frotz/src/common/process.c"
#include "../src/zmachine/frotz/src/common/random.c"
#include "../src/zmachine/frotz/src/common/redirect.c"
#include "../src/zmachine/frotz/src/common/screen.c"
#include "../src/zmachine/frotz/src/common/stream.c"
#include "../src/zmachine/frotz/src/common/table.c"
#include "../src/zmachine/frotz/src/common/text.c"
#include "../src/zmachine/frotz/src/common/variable.c"

// Frotz dumb interface
#include "../src/zmachine/frotz/src/dumb/dinit.c"
#include "../src/zmachine/frotz/src/dumb/dinput.c"
#include "../src/zmachine/frotz/src/dumb/doutput.c"

/**
 * Kernel entry point - called by TT-Metal
 */
void kernel_main() {
    // Get DRAM buffer addresses from runtime arguments
    uint32_t game_data_addr = get_arg_val<uint32_t>(0);
    uint32_t game_data_size = get_arg_val<uint32_t>(1);
    uint32_t input_addr     = get_arg_val<uint32_t>(2);
    uint32_t input_size     = get_arg_val<uint32_t>(3);
    uint32_t output_addr    = get_arg_val<uint32_t>(4);
    uint32_t output_size    = get_arg_val<uint32_t>(5);

    // Initialize Blackhole I/O with DRAM buffers
    blackhole_io_init(
        game_data_addr,
        game_data_size,
        input_addr,
        input_size,
        output_addr,
        output_size
    );

    // Prepare arguments for Frotz
    const char* game_name = "zork1.z3";
    char* argv[2];
    argv[0] = (char*)"zork";
    argv[1] = (char*)game_name;

    // RUN ZORK ON BLACKHOLE! 🚀
    frotz_main(2, argv);
}
