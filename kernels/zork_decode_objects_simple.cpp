// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_decode_objects_simple.cpp - Port of working Python decoder!
 * 
 * Based on the Python decoder that successfully decoded all 199 objects.
 * Simple algorithm: skip abbreviations, just decode characters directly.
 */

#include <cstdint>

typedef uint8_t zbyte;
typedef uint16_t zword;
typedef uint16_t zchar;

static zbyte* story_data;
static char* output_buffer;
static uint32_t output_pos;

static void outchar(char c) {
    if (output_pos < 3900) {
        output_buffer[output_pos++] = c;
    }
}

// Get character from alphabet (matching Python implementation)
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

// Simple decoder - exactly like Python version!
static void decode_zstring_simple(zword addr, uint32_t max_words) {
    int shift_state = 0;
    uint32_t words_read = 0;
    
    while (words_read < max_words && addr < 86000) {
        zword word = (story_data[addr] << 8) | story_data[addr + 1];
        addr += 2;
        words_read++;
        
        // Extract 3 characters (5 bits each)
        for (int shift = 10; shift >= 0; shift -= 5) {
            zbyte c = (word >> shift) & 0x1F;
            
            if (c == 0) {
                outchar(' ');
                shift_state = 0;
            } else if (c >= 6) {
                outchar(get_char(shift_state, c));
                shift_state = 0;
            } else if (c >= 4 && c <= 5) {
                shift_state = (c == 4) ? 1 : 2;
            }
            // Skip abbreviations (c=1,2,3) to avoid recursion
        }
        
        // Check end bit
        if (word & 0x8000) break;
    }
}

void kernel_main() {
    uint32_t game_data_dram = get_arg_val<uint32_t>(0);
    uint32_t output_dram = get_arg_val<uint32_t>(4);

    constexpr uint32_t L1_GAME = 0x10000;
    constexpr uint32_t L1_OUTPUT = 0x50000;
    constexpr uint32_t GAME_SIZE = 86838;
    constexpr uint32_t OUTPUT_SIZE = 4096;

    // Read game data in 1KB chunks
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

    // Header info
    zword obj_table = (story_data[0x0A] << 8) | story_data[0x0B];
    zword first_obj = obj_table + 62;  // Skip property defaults

    const char* header = "=== ZORK OBJECTS (SIMPLE DECODER)! ===\n\n";
    while (*header) outchar(*header++);

    // Decode first 30 objects (same as Python script initially showed)
    uint32_t decoded = 0;
    const uint32_t MAX_OBJECTS = 30;

    for (uint32_t obj_num = 1; obj_num <= MAX_OBJECTS; obj_num++) {
        zword entry_addr = first_obj + ((obj_num - 1) * 9);
        
        if (entry_addr >= GAME_SIZE - 10) break;

        // Get property table address (bytes 7-8 of object entry)
        zword prop_addr = (story_data[entry_addr + 7] << 8) | story_data[entry_addr + 8];
        
        if (prop_addr == 0 || prop_addr >= GAME_SIZE - 20) continue;

        // Get text length (first byte of property table)
        zbyte text_len = story_data[prop_addr];
        
        if (text_len > 0 && text_len < 30) {
            uint32_t save_pos = output_pos;
            
            // Object number
            if (obj_num >= 10) outchar('0' + (obj_num / 10));
            outchar('0' + (obj_num % 10));
            outchar('.');
            outchar(' ');
            
            // Decode name (start at prop_addr + 1, skip length byte)
            decode_zstring_simple(prop_addr + 1, text_len);
            
            // Check if we got reasonable output
            if (output_pos > save_pos + 3 && output_pos < save_pos + 100) {
                decoded++;
                outchar('\n');
            } else {
                output_pos = save_pos;  // Revert if bad
            }
        }
    }

    const char* msg = "\n--- Decoded ";
    while (*msg) outchar(*msg++);
    if (decoded >= 10) outchar('0' + (decoded / 10));
    outchar('0' + (decoded % 10));
    msg = " objects! ---\n";
    while (*msg) outchar(*msg++);
    outchar('\0');

    // Write output
    InterleavedAddrGen<true> out_gen = {
        .bank_base_address = output_dram,
        .page_size = OUTPUT_SIZE
    };
    uint64_t out_noc = get_noc_addr(0, out_gen);
    noc_async_write(L1_OUTPUT, out_noc, OUTPUT_SIZE);
    noc_async_write_barrier();
}
