// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_find_title.cpp - Find the "ZORK I" title text!
 *
 * Scan the entire game file looking for Z-strings containing "ZORK", "Great",
 * "Underground", "Empire" - the famous opening text.
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

static void decode_text(zword z_addr) {
    zbyte c, prev_c = 0;
    int shift_state = 0;
    int shift_lock = 0;
    int status = 0;
    zword addr = z_addr;
    zword code;

    do {
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
                    zword abbr_addr = (story_data[ptr_addr] << 8) | story_data[ptr_addr + 1];
                    abbr_addr *= 2;
                    decode_text(abbr_addr);
                    status = 0;
                }
                break;

            case 2:
                status = 3;
                break;

            case 3:
                outchar(translate_from_zscii((prev_c << 5) | c));
                status = 0;
                break;
            }
            prev_c = c;
        }
    } while (!(code & 0x8000));
}

// Simple substring search (case insensitive for ASCII)
static bool contains_word(const char* text, uint32_t len, const char* word) {
    uint32_t word_len = 0;
    while (word[word_len]) word_len++;

    for (uint32_t i = 0; i <= len - word_len; i++) {
        bool match = true;
        for (uint32_t j = 0; j < word_len; j++) {
            char c1 = text[i + j];
            char c2 = word[j];
            // Simple uppercase conversion
            if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
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
    char* output = (char*)L1_OUTPUT;
    uint32_t pos = 0;

    z_header.version = story_data[0];
    z_header.abbreviations = (story_data[0x18] << 8) | story_data[0x19];
    zword high_mem = (story_data[0x04] << 8) | story_data[0x05];

    const char* header = "=== SEARCHING FOR ZORK TITLE! ===\n\n";
    while (*header) output[pos++] = *header++;

    // Scan entire high memory for strings containing key words
    char temp_buffer[300];
    uint32_t matches_found = 0;
    const uint32_t MAX_MATCHES = 20;

    const char* msg = "Scanning from 0x";
    while (*msg) output[pos++] = *msg++;
    const char* hex = "0123456789ABCDEF";
    output[pos++] = hex[(high_mem >> 12) & 0xF];
    output[pos++] = hex[(high_mem >> 8) & 0xF];
    output[pos++] = hex[(high_mem >> 4) & 0xF];
    output[pos++] = hex[high_mem & 0xF];
    output[pos++] = '\n';
    output[pos++] = '\n';

    // Scan every word boundary in high memory
    for (uint32_t addr = high_mem; addr < GAME_SIZE - 20 && matches_found < MAX_MATCHES; addr += 2) {
        // Try to decode as Z-string
        uint32_t temp_pos = 0;
        output_ptr = temp_buffer;
        output_limit = 250;

        // Check if this looks like a string start
        zword first_word = (story_data[addr] << 8) | story_data[addr + 1];
        uint8_t c0 = (first_word >> 10) & 0x1F;
        uint8_t c1 = (first_word >> 5) & 0x1F;
        uint8_t c2 = first_word & 0x1F;

        // At least one printable char
        if ((c0 >= 6 && c0 <= 31) || (c1 >= 6 && c1 <= 31) || (c2 >= 6 && c2 <= 31) || c0 == 0 || c1 == 0 || c2 == 0) {
            decode_text(addr);
            uint32_t decoded_len = output_ptr - temp_buffer;

            if (decoded_len >= 10 && decoded_len < 250) {
                // Check for key words
                if (contains_word(temp_buffer, decoded_len, "ZORK") ||
                    contains_word(temp_buffer, decoded_len, "GREAT") ||
                    contains_word(temp_buffer, decoded_len, "UNDERGROUND") ||
                    contains_word(temp_buffer, decoded_len, "EMPIRE") ||
                    contains_word(temp_buffer, decoded_len, "INFOCOM") ||
                    contains_word(temp_buffer, decoded_len, "COPYRIGHT")) {

                    // Found a match! Print it
                    matches_found++;

                    // Print address
                    msg = "[0x";
                    while (*msg) output[pos++] = *msg++;
                    output[pos++] = hex[(addr >> 12) & 0xF];
                    output[pos++] = hex[(addr >> 8) & 0xF];
                    output[pos++] = hex[(addr >> 4) & 0xF];
                    output[pos++] = hex[addr & 0xF];
                    msg = "] ";
                    while (*msg) output[pos++] = *msg++;

                    // Copy decoded text
                    for (uint32_t i = 0; i < decoded_len && pos < 3900; i++) {
                        output[pos++] = temp_buffer[i];
                    }
                    output[pos++] = '\n';
                    output[pos++] = '\n';
                }
            }
        }
    }

    msg = "--- Found ";
    while (*msg) output[pos++] = *msg++;
    if (matches_found >= 10) output[pos++] = '0' + (matches_found / 10);
    output[pos++] = '0' + (matches_found % 10);
    msg = " title-related strings! ---\n";
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
