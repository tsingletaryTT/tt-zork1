// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_frotz_real.cpp - Use ACTUAL Frotz text decoding functions!
 *
 * Big bet: Stop rewriting! Use the proven Frotz code adapted for bare-metal.
 * Extracted from src/zmachine/frotz/src/common/text.c
 */

#include <cstdint>

// Z-machine types (from Frotz defs.h)
typedef uint8_t zbyte;
typedef uint16_t zword;
typedef uint16_t zchar;

// Z-machine header structure (minimal for text decoding)
struct {
    zword abbreviations;  // Offset 0x18 in header
    zbyte version;
} z_header;

// Output buffer
static char* output_ptr;
static uint32_t output_limit;

// Memory pointer
static zbyte* story_data;

// Helper macros (adapted from Frotz)
#define LOW_BYTE(addr, v) v = story_data[addr]
#define LOW_WORD(addr, v) v = (story_data[addr] << 8) | story_data[addr + 1]

// Output a character (replaces Frotz's screen output)
static void outchar(zchar c) {
    if (output_ptr && output_limit > 0) {
        *output_ptr++ = (char)c;
        output_limit--;
    }
}

// Alphabet function (from Frotz text.c)
static zchar alphabet(int set, int index) {
    // Version 3 uses default alphabets
    if (set == 0)
        return 'a' + index;
    else if (set == 1)
        return 'A' + index;
    else  // set == 2, v3 punctuation
        return " ^0123456789.,!?_#'\"/\\-:()"[index];
}

// Simplified translate_from_zscii (for v3, just return as-is for ASCII range)
static zchar translate_from_zscii(zbyte c) {
    return c;  // For v3 Zork, ASCII chars are fine
}

// ACTUAL Frotz decode_text function (adapted for bare-metal)
static void decode_text(zword z_addr) {
    zchar *ptr;
    zword code;
    zbyte c, prev_c = 0;
    int shift_state = 0;
    int shift_lock = 0;
    int status = 0;

    zword addr = z_addr;

    do {
        // Fetch the next 16bit word
        code = (story_data[addr] << 8) | story_data[addr + 1];
        addr += 2;

        // Read its three Z-characters
        for (int i = 10; i >= 0; i -= 5) {
            c = (code >> i) & 0x1f;

            switch (status) {
            case 0:  // normal operation
                if (shift_state == 2 && c == 6)
                    status = 2;
                else if (c >= 6)
                    outchar(alphabet(shift_state, c - 6));
                else if (c == 0)
                    outchar(' ');
                else if (z_header.version >= 2 && c == 1)
                    status = 1;  // abbreviation
                else if (z_header.version >= 3 && c <= 3)
                    status = 1;  // abbreviation
                else {
                    shift_state = (shift_lock + (c & 1) + 1) % 3;
                    if (z_header.version <= 2 && c >= 4)
                        shift_lock = shift_state;
                    break;
                }
                shift_state = shift_lock;
                break;

            case 1:  // abbreviation
                {
                    // Calculate abbreviation address
                    zword ptr_addr = z_header.abbreviations + 64 * (prev_c - 1) + 2 * c;
                    zword abbr_addr;
                    abbr_addr = (story_data[ptr_addr] << 8) | story_data[ptr_addr + 1];
                    // Abbreviations use packed addresses: multiply by 2 for v3
                    abbr_addr *= 2;
                    // Recursively decode abbreviation
                    decode_text(abbr_addr);
                    status = 0;
                }
                break;

            case 2:  // ZSCII character - first part
                status = 3;
                break;

            case 3:  // ZSCII character - second part
                {
                    zchar c2 = translate_from_zscii((prev_c << 5) | c);
                    outchar(c2);
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

    // Read game into L1
    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = game_data_dram,
        .page_size = GAME_SIZE
    };
    uint64_t game_noc = get_noc_addr(0, game_gen);
    noc_async_read(game_noc, L1_GAME, GAME_SIZE);
    noc_async_read_barrier();

    // Setup global pointers
    story_data = (zbyte*)L1_GAME;
    char* output = (char*)L1_OUTPUT;
    uint32_t pos = 0;

    // Initialize z_header from game file
    z_header.version = story_data[0];
    z_header.abbreviations = (story_data[0x18] << 8) | story_data[0x19];

    const char* header = "=== REAL FROTZ DECODER ON BLACKHOLE! ===\n\n";
    while (*header) output[pos++] = *header++;

    const char* msg = "Version: ";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = '0' + z_header.version;
    output[pos++] = '\n';

    msg = "Abbreviations at: 0x";
    while (*msg) output[pos++] = *msg++;
    const char* hex = "0123456789ABCDEF";
    output[pos++] = hex[(z_header.abbreviations >> 12) & 0xF];
    output[pos++] = hex[(z_header.abbreviations >> 8) & 0xF];
    output[pos++] = hex[(z_header.abbreviations >> 4) & 0xF];
    output[pos++] = hex[z_header.abbreviations & 0xF];
    output[pos++] = '\n';
    output[pos++] = '\n';

    // Scan high memory for strings
    zword high_mem = (story_data[0x04] << 8) | story_data[0x05];

    msg = "Scanning from 0x";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = hex[(high_mem >> 12) & 0xF];
    output[pos++] = hex[(high_mem >> 8) & 0xF];
    output[pos++] = hex[(high_mem >> 4) & 0xF];
    output[pos++] = hex[high_mem & 0xF];
    output[pos++] = ':';
    output[pos++] = '\n';
    output[pos++] = '\n';

    // Scan for valid Z-strings in high memory
    uint32_t strings_found = 0;
    const uint32_t MAX_STRINGS = 5;

    for (uint32_t addr = high_mem; addr < GAME_SIZE - 20 && strings_found < MAX_STRINGS; addr += 2) {
        // Check if this looks like a Z-string start
        zword first_word = (story_data[addr] << 8) | story_data[addr + 1];

        // Simple heuristic: first word should have some valid characters
        uint8_t c0 = (first_word >> 10) & 0x1F;
        uint8_t c1 = (first_word >> 5) & 0x1F;
        uint8_t c2 = first_word & 0x1F;

        // At least one char should be a letter (6-31)
        if ((c0 >= 6 && c0 <= 31) || (c1 >= 6 && c1 <= 31) || (c2 >= 6 && c2 <= 31)) {
            uint32_t save_pos = pos;

            // Try decoding
            output_ptr = output + pos;
            output_limit = 3900 - pos;

            decode_text(addr);

            // Check if we decoded something reasonable
            uint32_t decoded_len = (output_ptr - (output + pos));
            if (decoded_len >= 5 && decoded_len < 200) {
                strings_found++;
                pos = output_ptr - output;
                output[pos++] = '\n';
                output[pos++] = '\n';
            } else {
                pos = save_pos;  // Revert
            }
        }
    }

    msg = "--- Decoded ";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = '0' + (strings_found / 10);
    output[pos++] = '0' + (strings_found % 10);
    msg = " strings with REAL Frotz code! ---\n";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = '\0';

    // Write output
    InterleavedAddrGen<true> out_gen = {
        .bank_base_address = output_dram,
        .page_size = OUTPUT_SIZE
    };
    uint64_t out_noc = get_noc_addr(0, out_gen);
    noc_async_write(L1_OUTPUT, out_noc, OUTPUT_SIZE);
    noc_async_write_barrier();
}
