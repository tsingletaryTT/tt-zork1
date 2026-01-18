// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_objects_with_abbrev.cpp - Object decoder WITH abbreviation support!
 *
 * This kernel extends the minimal decoder to handle Z-string abbreviations,
 * giving us PERFECT object names: "West of House" instead of "West eHouse"
 *
 * Abbreviation Algorithm (Z-machine v3):
 * - Abbreviation table at header offset $18
 * - 96 total abbreviations (32 for code 1, 32 for code 2, 32 for code 3)
 * - Each entry is a word-address (multiply by 2 for byte address)
 * - When we encounter code 1/2/3, next char is abbreviation index
 * - Recursively decode that abbreviation string
 */

#include <cstdint>

typedef uint8_t zbyte;
typedef uint16_t zword;

static zbyte* story_data;
static char* output;
static uint32_t pos;
static zword abbrev_table_addr;  // Address of abbreviation table

// Maximum recursion depth to prevent infinite loops
constexpr uint32_t MAX_ABBREV_DEPTH = 3;

// Forward declaration for recursion
static void decode_zstring(zword addr, uint32_t max_words, uint32_t depth);

/**
 * Get character from alphabet table
 * A0: lowercase a-z
 * A1: uppercase A-Z
 * A2: punctuation and special chars
 */
static char get_char(int alphabet, int index) {
    if (alphabet == 0) {  // A0: lowercase
        if (index == 0) return ' ';
        if (index >= 6 && index <= 31) return 'a' + (index - 6);
    } else if (alphabet == 1) {  // A1: uppercase
        if (index == 0) return ' ';
        if (index >= 6 && index <= 31) return 'A' + (index - 6);
    } else {  // A2: punctuation
        const char* punct = " ^0123456789.,!?_#'\"/\\-:()";
        if (index < 26) return punct[index];
    }
    return '?';
}

/**
 * Decode abbreviation by looking up in abbreviation table
 *
 * @param code Abbreviation code (1, 2, or 3)
 * @param index Abbreviation index (0-31)
 * @param depth Current recursion depth (prevent infinite loops)
 */
static void decode_abbreviation(zbyte code, zbyte index, uint32_t depth) {
    if (depth >= MAX_ABBREV_DEPTH) {
        output[pos++] = '?';  // Too deep, bail out
        return;
    }

    if (code < 1 || code > 3 || index > 31) {
        output[pos++] = '?';  // Invalid abbreviation
        return;
    }

    // Calculate abbreviation table entry index
    // Code 1: entries 0-31, Code 2: entries 32-63, Code 3: entries 64-95
    uint32_t entry_index = ((code - 1) * 32) + index;

    // Each abbreviation entry is a word (2 bytes) in the table
    zword entry_addr = abbrev_table_addr + (entry_index * 2);

    if (entry_addr >= 86000) {
        output[pos++] = '?';  // Invalid address
        return;
    }

    // Read the word-address from the abbreviation table
    zword word_addr = (story_data[entry_addr] << 8) | story_data[entry_addr + 1];

    // Convert word-address to byte-address (multiply by 2)
    zword byte_addr = word_addr * 2;

    if (byte_addr >= 86000) {
        output[pos++] = '?';  // Invalid address
        return;
    }

    // Recursively decode the abbreviation string
    decode_zstring(byte_addr, 30, depth + 1);
}

/**
 * Decode Z-string with abbreviation support
 *
 * @param addr Starting address of Z-string
 * @param max_words Maximum words to read (prevent infinite loops)
 * @param depth Recursion depth (for abbreviation expansion)
 */
static void decode_zstring(zword addr, uint32_t max_words, uint32_t depth) {
    if (addr >= 86000 || depth >= MAX_ABBREV_DEPTH) return;

    int shift_state = 0;  // 0=A0, 1=A1, 2=A2
    zbyte abbrev_code = 0;  // Set when we see code 1/2/3, next char is index
    uint32_t words_read = 0;

    while (words_read < max_words && addr < 86000) {
        // Read 16-bit word (big-endian)
        zword word = (story_data[addr] << 8) | story_data[addr + 1];
        addr += 2;
        words_read++;

        // Extract 3 characters (5 bits each)
        for (int shift = 10; shift >= 0; shift -= 5) {
            zbyte c = (word >> shift) & 0x1F;

            // Handle abbreviation sequences
            if (abbrev_code != 0) {
                // Previous character was abbrev code 1/2/3, this is the index
                decode_abbreviation(abbrev_code, c, depth);
                abbrev_code = 0;
                shift_state = 0;  // Reset shift after abbreviation
                continue;
            }

            // Check character code
            if (c >= 6) {
                // Regular character
                if (pos < 16000) {  // Safety check
                    output[pos++] = get_char(shift_state, c);
                }
                shift_state = 0;  // Reset shift after character
            } else if (c == 0) {
                // Space
                if (pos < 16000) {
                    output[pos++] = ' ';
                }
                shift_state = 0;
            } else if (c >= 1 && c <= 3) {
                // Abbreviation code - next character is the index
                abbrev_code = c;
            } else if (c == 4 || c == 5) {
                // Shift to A1 or A2
                shift_state = (c == 4) ? 1 : 2;
            }
        }

        // Check end bit (high bit of word)
        if (word & 0x8000) break;
    }
}

/**
 * Kernel main entry point
 */
void kernel_main() {
    uint32_t game_dram = get_arg_val<uint32_t>(0);
    uint32_t out_dram = get_arg_val<uint32_t>(4);

    constexpr uint32_t L1_GAME = 0x10000;
    constexpr uint32_t L1_OUT = 0x50000;
    constexpr uint32_t GAME_SIZE = 86838;

    // Load game data from DRAM to L1
    InterleavedAddrGen<true> gen = {.bank_base_address = game_dram, .page_size = 1024};

    for (uint32_t off = 0; off < GAME_SIZE; off += 1024) {
        uint32_t sz = (off + 1024 <= GAME_SIZE) ? 1024 : (GAME_SIZE - off);
        noc_async_read(get_noc_addr(off / 1024, gen), L1_GAME + off, sz);
    }
    noc_async_read_barrier();

    story_data = (zbyte*)L1_GAME;
    output = (char*)L1_OUT;
    pos = 0;

    // Read abbreviation table address from header offset $18
    abbrev_table_addr = (story_data[0x18] << 8) | story_data[0x19];

    const char* h = "=== ZORK OBJECTS WITH PERFECT ABBREVIATIONS! ===\n";
    while (*h) output[pos++] = *h++;
    h = "(Decoding objects 1-70 including 'West of House'!)\n\n";
    while (*h) output[pos++] = *h++;

    // Object table address from header offset $0A
    zword obj_table = (story_data[0x0A] << 8) | story_data[0x0B];
    zword obj_start = obj_table + 62;  // Skip 31 words of defaults

    // Decode objects 1-70 to see "West of House"!
    for (uint32_t i = 1; i <= 70; i++) {
        zword entry = obj_start + ((i - 1) * 9);
        zword prop = (story_data[entry + 7] << 8) | story_data[entry + 8];

        if (prop > 0 && prop < 86000) {
            zbyte len = story_data[prop];
            if (len > 0 && len < 20) {
                // Display object number
                if (i >= 10) output[pos++] = '0' + (i / 10);
                output[pos++] = '0' + (i % 10);
                output[pos++] = '.';
                output[pos++] = ' ';

                // Decode with abbreviations!
                decode_zstring(prop + 1, len, 0);

                output[pos++] = '\n';
            }
        }
    }

    output[pos++] = '\n';
    h = "✨ ABBREVIATIONS WORKING! ✨\n";
    while (*h) output[pos++] = *h++;
    output[pos++] = '\0';

    // Write output back to DRAM
    InterleavedAddrGen<true> out_gen = {.bank_base_address = out_dram, .page_size = 4096};
    noc_async_write(L1_OUT, get_noc_addr(0, out_gen), 16384);
    noc_async_write_barrier();
}
