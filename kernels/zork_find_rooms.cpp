// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_find_rooms.cpp - BRUTE FORCE search for room names!
 * 
 * Try decoding EVERY object (1-255) and look for keywords:
 * - "West" "House" "North" "South" "East"  
 * - "mailbox" "leaflet" "lamp"
 * - "ZORK" "Great" "Empire"
 * 
 * Report which object numbers contain these strings!
 */

#include <cstdint>

typedef uint8_t zbyte;
typedef uint16_t zword;
typedef uint16_t zchar;

struct {
    zword abbreviations;
    zbyte version;
    zword object_table;
} z_header;

static zbyte* story_data;
static char decode_buffer[200];  // Temp buffer for each object
static uint32_t decode_pos;

static void outchar(zchar c) {
    if (decode_pos < 199) {
        decode_buffer[decode_pos++] = (char)c;
    }
}

static zchar alphabet(int set, int index) {
    if (set == 0) return 'a' + index;
    else if (set == 1) return 'A' + index;
    else return " ^0123456789.,!?_#'\"/\\-:()"[index];
}

// Simple substring search (case-insensitive)
static bool contains(const char* text, uint32_t len, const char* word) {
    uint32_t word_len = 0;
    while (word[word_len]) word_len++;
    
    for (uint32_t i = 0; i <= len - word_len; i++) {
        bool match = true;
        for (uint32_t j = 0; j < word_len; j++) {
            char c1 = text[i + j];
            char c2 = word[j];
            if (c1 >= 'a' && c1 <= 'z') c1 -= 32;  // Uppercase
            if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
            if (c1 != c2) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

// Minimal decoder - skip abbreviations to avoid hangs!
static void decode_simple(zword z_addr) {
    if (z_addr >= 86000) return;
    
    int shift_state = 0;
    zword addr = z_addr;
    zword code;
    uint32_t words_read = 0;

    do {
        if (words_read++ >= 30) break;
        if (addr >= 86000) break;
        
        code = (story_data[addr] << 8) | story_data[addr + 1];
        addr += 2;

        for (int i = 10; i >= 0; i -= 5) {
            zbyte c = (code >> i) & 0x1f;

            if (c == 0) {
                outchar(' ');
            } else if (c >= 6 && c <= 31) {
                outchar(alphabet(shift_state, c - 6));
                shift_state = 0;  // Reset after char
            } else if (c >= 4 && c <= 5) {
                shift_state = (c == 4) ? 1 : 2;  // Shift to A1 or A2
            }
            // Skip abbreviations (c=1,2,3) to avoid recursion!
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
    char* output = (char*)L1_OUTPUT;
    uint32_t pos = 0;

    z_header.version = story_data[0];
    z_header.abbreviations = (story_data[0x18] << 8) | story_data[0x19];
    z_header.object_table = (story_data[0x0A] << 8) | story_data[0x0B];

    const char* header = "=== BRUTE FORCE ROOM SEARCH! ===\n\n";
    while (*header) output[pos++] = *header++;

    zword obj_entry = z_header.object_table + 62;

    // Search ALL objects (1-150) for keywords!
    const char* keywords[] = {
        "West", "House", "North", "South", "East",
        "mailbox", "leaflet", "lamp", "door",
        "ZORK", "Great", "Empire", "Infocom"
    };
    const uint32_t num_keywords = 13;

    for (uint32_t obj_num = 1; obj_num <= 50; obj_num++) {
        zword entry_addr = obj_entry + ((obj_num - 1) * 9);
        
        if (entry_addr >= GAME_SIZE - 10) break;

        zword prop_table_addr = (story_data[entry_addr + 7] << 8) | story_data[entry_addr + 8];
        
        if (prop_table_addr == 0 || prop_table_addr >= GAME_SIZE - 10) continue;

        zbyte text_len = story_data[prop_table_addr];
        
        if (text_len > 0 && text_len < 30) {
            // Try to decode this object!
            decode_pos = 0;
            decode_simple(prop_table_addr + 1);
            decode_buffer[decode_pos] = '\0';

            // Check if it contains any keywords!
            for (uint32_t k = 0; k < num_keywords; k++) {
                if (contains(decode_buffer, decode_pos, keywords[k])) {
                    // FOUND IT!
                    const char* msg = "Object ";
                    while (*msg) output[pos++] = *msg++;
                    if (obj_num >= 100) output[pos++] = '0' + (obj_num / 100);
                    if (obj_num >= 10) output[pos++] = '0' + ((obj_num % 100) / 10);
                    output[pos++] = '0' + (obj_num % 10);
                    msg = ": ";
                    while (*msg) output[pos++] = *msg++;
                    
                    // Copy decoded text
                    for (uint32_t i = 0; i < decode_pos && pos < 3900; i++) {
                        output[pos++] = decode_buffer[i];
                    }
                    output[pos++] = '\n';
                    break;  // Don't report same object multiple times
                }
            }
        }
    }

    const char* msg = "\n--- Search complete! ---\n";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = '\0';

    InterleavedAddrGen<true> out_gen = {
        .bank_base_address = output_dram,
        .page_size = OUTPUT_SIZE
    };
    uint64_t out_noc = get_noc_addr(0, out_gen);
    noc_async_write(L1_OUTPUT, out_noc, OUTPUT_SIZE);
    noc_async_write_barrier();
}
