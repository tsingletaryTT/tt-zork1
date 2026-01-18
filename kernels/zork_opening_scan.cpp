// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_opening_scan.cpp - Fast scan for opening text
 *
 * Scan ONLY the first 2000 bytes of high memory for PRINT instructions.
 * This should find the opening text without hanging!
 */

#include <cstdint>

typedef uint8_t zbyte;
typedef uint16_t zword;
typedef uint16_t zchar;

struct {
    zword abbreviations;
    zbyte version;
} z_header;

static zbyte* story_data;
static char* output_buffer;
static uint32_t output_pos;

static void outchar(zchar c) {
    if (output_pos < 3900) {
        output_buffer[output_pos++] = (char)c;
    }
}

static zchar alphabet(int set, int index) {
    if (set == 0) return 'a' + index;
    else if (set == 1) return 'A' + index;
    else return " ^0123456789.,!?_#'\"/\\-:()"[index];
}

static void decode_text_safe(zword z_addr, uint32_t max_words) {
    zbyte c, prev_c = 0;
    int shift_state = 0;
    int shift_lock = 0;
    int status = 0;
    zword addr = z_addr;
    zword code;
    uint32_t words_read = 0;

    do {
        if (words_read++ >= max_words) break;  // Safety limit!
        if (addr >= 86000) break;  // Don't read past game file

        code = (story_data[addr] << 8) | story_data[addr + 1];
        addr += 2;

        for (int i = 10; i >= 0; i -= 5) {
            c = (code >> i) & 0x1f;

            switch (status) {
            case 0:
                if (shift_state == 2 && c == 6)
                    status = 2;
                else if (c >= 6)
                    outchar(alphabet(shift_state, c - 6));
                else if (c == 0)
                    outchar(' ');
                else if (z_header.version >= 3 && c <= 3)
                    status = 1;
                else {
                    shift_state = (shift_lock + (c & 1) + 1) % 3;
                    break;
                }
                shift_state = shift_lock;
                break;

            case 1:  // abbreviation
                {
                    zword ptr_addr = z_header.abbreviations + 64 * (prev_c - 1) + 2 * c;
                    if (ptr_addr < 86000) {
                        zword abbr_addr = (story_data[ptr_addr] << 8) | story_data[ptr_addr + 1];
                        abbr_addr *= 2;
                        if (abbr_addr < 86000) {
                            decode_text_safe(abbr_addr, 20);  // Limit recursion
                        }
                    }
                    status = 0;
                }
                break;

            case 2:
                status = 3;
                break;

            case 3:
                {
                    zbyte zscii = (prev_c << 5) | c;
                    if (zscii >= 32 && zscii < 127) {
                        outchar(zscii);
                    }
                    status = 0;
                }
                break;
            }
            prev_c = c;
        }
    } while (!(code & 0x8000));
}

void kernel_main() {
    uint32_t game_data_dram = get_arg_val<uint32_t>(0);
    uint32_t output_dram = get_arg_val<uint32_t>(4);

    constexpr uint32_t L1_GAME = 0x10000;
    constexpr uint32_t L1_OUTPUT = 0x50000;
    constexpr uint32_t GAME_SIZE = 86838;
    constexpr uint32_t OUTPUT_SIZE = 4096;

    // Read game data
    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = game_data_dram,
        .page_size = 1024
    };

    for (uint32_t offset = 0; offset < GAME_SIZE; offset += 1024) {
        uint32_t chunk_size = (offset + 1024 <= GAME_SIZE) ? 1024 : (GAME_SIZE - offset);
        uint64_t game_noc = get_noc_addr(offset / 1024, game_gen);
        noc_async_read(game_noc, L1_GAME + offset, chunk_size);
    }
    noc_async_read_barrier();

    story_data = (zbyte*)L1_GAME;
    output_buffer = (char*)L1_OUTPUT;
    output_pos = 0;

    z_header.version = story_data[0];
    z_header.abbreviations = (story_data[0x18] << 8) | story_data[0x19];
    zword high_mem = (story_data[0x04] << 8) | story_data[0x05];

    const char* header = "=== SCANNING EARLY HIGH MEMORY! ===\n\n";
    while (*header) output_buffer[output_pos++] = *header++;

    const char* msg = "High memory: 0x";
    while (*msg) output_buffer[output_pos++] = *msg++;
    const char* hex = "0123456789ABCDEF";
    output_buffer[output_pos++] = hex[(high_mem >> 12) & 0xF];
    output_buffer[output_pos++] = hex[(high_mem >> 8) & 0xF];
    output_buffer[output_pos++] = hex[(high_mem >> 4) & 0xF];
    output_buffer[output_pos++] = hex[high_mem & 0xF];
    output_buffer[output_pos++] = '\n';
    output_buffer[output_pos++] = '\n';

    // Scan ONLY first 2000 bytes for PRINT instructions
    uint32_t prints_found = 0;
    const uint32_t MAX_PRINTS = 15;
    const uint32_t SCAN_END = high_mem + 2000;

    for (uint32_t addr = high_mem; addr < SCAN_END && addr < GAME_SIZE - 10 && prints_found < MAX_PRINTS; addr++) {
        zbyte opcode = story_data[addr];

        if (opcode == 0xB2 || opcode == 0xB3) {
            uint32_t save_pos = output_pos;

            // Try to decode
            decode_text_safe(addr + 1, 50);  // Max 50 words

            uint32_t decoded_len = output_pos - save_pos;
            if (decoded_len >= 5 && decoded_len < 300) {
                // Valid!
                prints_found++;
                output_buffer[output_pos++] = '\n';
                output_buffer[output_pos++] = '\n';
            } else {
                // Revert
                output_pos = save_pos;
            }
        }
    }

    msg = "--- Found ";
    while (*msg) output_buffer[output_pos++] = *msg++;
    if (prints_found >= 10) output_buffer[output_pos++] = '0' + (prints_found / 10);
    output_buffer[output_pos++] = '0' + (prints_found % 10);
    msg = " strings ---\n";
    while (*msg) output_buffer[output_pos++] = *msg++;
    output_buffer[output_pos++] = '\0';

    // Write output
    InterleavedAddrGen<true> out_gen = {
        .bank_base_address = output_dram,
        .page_size = OUTPUT_SIZE
    };
    uint64_t out_noc = get_noc_addr(0, out_gen);
    noc_async_write(L1_OUTPUT, out_noc, OUTPUT_SIZE);
    noc_async_write_barrier();
}
