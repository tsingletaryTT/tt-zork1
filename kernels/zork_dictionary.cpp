// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_dictionary.cpp - Decode Zork's dictionary!
 *
 * HOLISTIC APPROACH: The dictionary is a STRUCTURED data with KNOWN valid text.
 * Dictionary format (v3):
 * - Address at header $08
 * - Starts with separator chars
 * - Then entry length byte
 * - Then number of entries (word)
 * - Then entries: each has 4 bytes of encoded text (6 Z-characters)
 *
 * This is GUARANTEED real Zork vocabulary!
 */

#include <cstdint>

typedef uint8_t zbyte;
typedef uint16_t zword;
typedef uint16_t zchar;

struct {
    zword abbreviations;
    zbyte version;
} z_header;

static char* output_ptr;
static uint32_t output_limit;
static zbyte* story_data;

#define LOW_BYTE(addr, v) v = story_data[addr]
#define LOW_WORD(addr, v) v = (story_data[addr] << 8) | story_data[addr + 1]

static void outchar(zchar c) {
    if (output_ptr && output_limit > 0) {
        *output_ptr++ = (char)c;
        output_limit--;
    }
}

static zchar alphabet(int set, int index) {
    if (set == 0) return 'a' + index;
    else if (set == 1) return 'A' + index;
    else return " ^0123456789.,!?_#'\"/\\-:()"[index];
}

static zchar translate_from_zscii(zbyte c) {
    return c;
}

// Decode Z-string (with abbreviations)
static void decode_text(zword z_addr) {
    zbyte c, prev_c = 0;
    int shift_state = 0;
    int shift_lock = 0;
    int status = 0;
    zword addr = z_addr;
    zword code;  // Declare here for scope

    do {
        code = (story_data[addr] << 8) | story_data[addr + 1];
        addr += 2;

        for (int i = 10; i >= 0; i -= 5) {
            c = (code >> i) & 0x1f;

            switch (status) {
            case 0:  // normal
                if (shift_state == 2 && c == 6)
                    status = 2;
                else if (c >= 6)
                    outchar(alphabet(shift_state, c - 6));
                else if (c == 0)
                    outchar(' ');
                else if (z_header.version >= 3 && c <= 3)
                    status = 1;  // abbreviation
                else {
                    shift_state = (shift_lock + (c & 1) + 1) % 3;
                    break;
                }
                shift_state = shift_lock;
                break;

            case 1:  // abbreviation
                {
                    zword ptr_addr = z_header.abbreviations + 64 * (prev_c - 1) + 2 * c;
                    zword abbr_addr = (story_data[ptr_addr] << 8) | story_data[ptr_addr + 1];
                    abbr_addr *= 2;  // v3 packed address
                    decode_text(abbr_addr);
                    status = 0;
                }
                break;

            case 2:  // ZSCII first part
                status = 3;
                break;

            case 3:  // ZSCII second part
                outchar(translate_from_zscii((prev_c << 5) | c));
                status = 0;
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

    // CRITICAL: page_size must match the buffer config (1024 bytes)!
    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = game_data_dram,
        .page_size = 1024  // Must match host buffer page_size!
    };

    // Read the full game data in chunks
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

    const char* header = "=== ZORK DICTIONARY FROM BLACKHOLE! ===\n\n";
    while (*header) output[pos++] = *header++;

    // Read dictionary address
    zword dict_addr = (story_data[0x08] << 8) | story_data[0x09];

    const char* msg = "Dictionary at: 0x";
    while (*msg) output[pos++] = *msg++;
    const char* hex = "0123456789ABCDEF";
    output[pos++] = hex[(dict_addr >> 12) & 0xF];
    output[pos++] = hex[(dict_addr >> 8) & 0xF];
    output[pos++] = hex[(dict_addr >> 4) & 0xF];
    output[pos++] = hex[dict_addr & 0xF];
    output[pos++] = '\n';
    output[pos++] = '\n';

    // Parse dictionary structure
    // Byte 0: number of separator chars
    zbyte num_seps = story_data[dict_addr];
    zword entry_addr = dict_addr + 1 + num_seps;  // Skip separators

    // Entry length
    zbyte entry_len = story_data[entry_addr++];

    // Number of entries
    zword num_entries = (story_data[entry_addr] << 8) | story_data[entry_addr + 1];
    entry_addr += 2;

    msg = "Entries: ";
    while (*msg) output[pos++] = *msg++;
    if (num_entries >= 100) output[pos++] = '0' + (num_entries / 100);
    if (num_entries >= 10) output[pos++] = '0' + ((num_entries % 100) / 10);
    output[pos++] = '0' + (num_entries % 10);
    output[pos++] = '\n';
    output[pos++] = '\n';

    // Decode 100 words to find "mailbox", "lamp", "leaflet", etc!
    uint32_t words_shown = 0;
    const uint32_t MAX_WORDS = 100;

    for (uint32_t i = 0; i < num_entries && words_shown < MAX_WORDS; i++) {
        // Each entry: 4 bytes encoded text in v3
        zword entry_start = entry_addr + (i * entry_len);

        // The encoded text is 4 bytes (2 words) = 6 Z-characters
        // We need to construct a temporary buffer with end bit set
        zbyte temp_string[4];
        temp_string[0] = story_data[entry_start];
        temp_string[1] = story_data[entry_start + 1];
        temp_string[2] = story_data[entry_start + 2];
        temp_string[3] = story_data[entry_start + 3] | 0x80;  // Set end bit on last word

        // Copy to a scratch location in L1
        zbyte* scratch = story_data + 65000;  // Use end of buffer
        scratch[0] = temp_string[0];
        scratch[1] = temp_string[1];
        scratch[2] = temp_string[2];
        scratch[3] = temp_string[3];

        // Decode
        output_ptr = output + pos;
        output_limit = 3900 - pos;

        decode_text(65000);  // Decode from scratch location

        uint32_t len = (output_ptr - (output + pos));
        if (len > 0 && len < 20) {  // Valid word length
            words_shown++;
            pos = output_ptr - output;
            output[pos++] = '\n';
        } else {
            // Skip invalid
        }
    }

    output[pos++] = '\n';
    msg = "--- Decoded ";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = '0' + (words_shown / 10);
    output[pos++] = '0' + (words_shown % 10);
    msg = " dictionary words! ---\n";
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
