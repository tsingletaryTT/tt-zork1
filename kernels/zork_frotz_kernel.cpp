// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_frotz_kernel.cpp - Frotz Z-machine interpreter on RISC-V!
 *
 * Strategy: Include Frotz's core interpreter and compile it for RISC-V.
 * This gives us all 119 Z-machine opcodes for free!
 *
 * We need:
 * 1. Frotz core files (process.c, text.c, fastmem.c, etc.)
 * 2. Minimal OS interface (os_init, os_display_string, etc.)
 * 3. Z-machine memory setup
 * 4. Call interpret() to run the game!
 */

#include <cstdint>
#include <cstring>

// Z-machine types (must match Frotz's frotz.h)
typedef uint8_t zbyte;
typedef uint16_t zword;

// Frotz global state (declared in Frotz but we provide storage)
extern "C" {
    // These are declared in Frotz's frotz.h
    extern zbyte* zmp;           // Pointer to Z-machine memory
    extern zbyte* pcp;           // Program counter pointer
    extern zword stack[];        // Z-machine stack
    extern zword* sp;            // Stack pointer
    extern int finished;         // Game finished flag

    // OS interface functions we must provide
    void os_init_screen(void);
    void os_reset_screen(void);
    void os_quit(int);
    void os_display_string(const zbyte*);
    void os_display_char(zbyte);
    void os_fatal(const char*);
    void os_tick(void);
    int os_read_line(int, zbyte*, int, int, int);
    int os_read_key(int, int);

    // Frotz main functions
    void init_memory(void);
    void interpret(void);
}

// Our output buffer
static char* output_buffer;
static uint32_t output_pos;
static uint32_t output_limit;

// Game memory
static zbyte game_memory[87000];  // Z-machine memory

/**
 * Minimal OS interface implementations
 */

void os_init_screen(void) {
    // Nothing needed for headless mode
}

void os_reset_screen(void) {
    // Nothing needed
}

void os_quit(int status) {
    finished = 1;  // Stop interpreter
}

void os_display_string(const zbyte* s) {
    // Copy string to output buffer
    while (*s && output_pos < output_limit) {
        output_buffer[output_pos++] = *s++;
    }
}

void os_display_char(zbyte c) {
    if (output_pos < output_limit) {
        output_buffer[output_pos++] = c;
    }
}

void os_fatal(const char* msg) {
    // Fatal error - copy message and quit
    const char* prefix = "\n[FATAL] ";
    while (*prefix && output_pos < output_limit) {
        output_buffer[output_pos++] = *prefix++;
    }
    while (*msg && output_pos < output_limit) {
        output_buffer[output_pos++] = *msg++;
    }
    finished = 1;
}

void os_tick(void) {
    // Called periodically by interpreter - do nothing
}

int os_read_line(int max, zbyte* buf, int timeout, int width, int continued) {
    // For now, return empty input (game will keep prompting)
    buf[0] = 0;
    return 0;
}

int os_read_key(int timeout, int show_cursor) {
    // For now, return 0 (no input)
    return 0;
}

/**
 * Kernel main - Run Frotz!
 */
void kernel_main() {
    uint32_t game_dram = get_arg_val<uint32_t>(0);
    uint32_t out_dram = get_arg_val<uint32_t>(4);

    constexpr uint32_t L1_GAME = 0x10000;
    constexpr uint32_t L1_OUT = 0x50000;

    // Load game data from DRAM to L1
    InterleavedAddrGen<true> gen = {.bank_base_address = game_dram, .page_size = 1024};
    for (uint32_t off = 0; off < 86838; off += 1024) {
        uint32_t sz = (off + 1024 <= 86838) ? 1024 : (86838 - off);
        noc_async_read(get_noc_addr(off / 1024, gen), L1_GAME + off, sz);
    }
    noc_async_read_barrier();

    // Copy game data to our memory buffer
    zbyte* loaded_game = (zbyte*)L1_GAME;
    for (uint32_t i = 0; i < 86838; i++) {
        game_memory[i] = loaded_game[i];
    }

    // Set up output buffer
    output_buffer = (char*)L1_OUT;
    output_pos = 0;
    output_limit = 15000;

    const char* h = "╔════════════════════════════════════════════════════╗\n";
    while (*h) output_buffer[output_pos++] = *h++;
    h = "║  FROTZ Z-MACHINE ON BLACKHOLE RISC-V!            ║\n";
    while (*h) output_buffer[output_pos++] = *h++;
    h = "╚════════════════════════════════════════════════════╝\n\n";
    while (*h) output_buffer[output_pos++] = *h++;

    // Initialize Frotz
    zmp = game_memory;       // Point Frotz at our memory
    finished = 0;            // Not finished yet

    // Initialize Z-machine memory structures
    // (In full Frotz, this is done by init_memory() and init_process())

    h = "Initializing Frotz interpreter...\n";
    while (*h) output_buffer[output_pos++] = *h++;

    // TODO: Call init_memory() and interpret()
    // For now, just show that we're ready
    h = "\n✓ Frotz ready to run on RISC-V!\n";
    while (*h) output_buffer[output_pos++] = *h++;
    h = "  (Need to link Frotz .o files)\n";
    while (*h) output_buffer[output_pos++] = *h++;

    output_buffer[output_pos++] = '\0';

    // Write output back to DRAM
    InterleavedAddrGen<true> out_gen = {.bank_base_address = out_dram, .page_size = 4096};
    noc_async_write(L1_OUT, get_noc_addr(0, out_gen), 16384);
    noc_async_write_barrier();
}
