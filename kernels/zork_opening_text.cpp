// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_opening_text.cpp - Decode Zork's opening text!
 *
 * Execute from PC and find PRINT instructions to decode the game's intro.
 * This should give us the famous "ZORK I" title and opening description!
 */

#include <cstdint>

typedef uint8_t zbyte;
typedef uint16_t zword;
typedef uint16_t zchar;

// Z-machine header
struct {
    zword abbreviations;
    zbyte version;
} z_header;

// Output system
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

// Real Frotz decode_text with abbreviation support
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
                    abbr_addr *= 2;
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

// Simple instruction executor - just look for PRINT opcodes
void kernel_main() {
    uint32_t game_data_dram = get_arg_val<uint32_t>(0);
    uint32_t output_dram = get_arg_val<uint32_t>(4);

    constexpr uint32_t L1_GAME = 0x10000;
    constexpr uint32_t L1_OUTPUT = 0x50000;
    constexpr uint32_t GAME_SIZE = 86838;
    constexpr uint32_t OUTPUT_SIZE = 4096;

    // Read game data with correct page_size
    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = game_data_dram,
        .page_size = 1024  // Must match host buffer!
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

    // Initialize Z-machine header
    z_header.version = story_data[0];
    z_header.abbreviations = (story_data[0x18] << 8) | story_data[0x19];
    zword pc = (story_data[0x06] << 8) | story_data[0x07];

    const char* header = "=== ZORK OPENING TEXT! ===\n\n";
    while (*header) output[pos++] = *header++;

    const char* msg = "Scanning from PC: 0x";
    while (*msg) output[pos++] = *msg++;
    const char* hex = "0123456789ABCDEF";
    output[pos++] = hex[(pc >> 12) & 0xF];
    output[pos++] = hex[(pc >> 8) & 0xF];
    output[pos++] = hex[(pc >> 4) & 0xF];
    output[pos++] = hex[pc & 0xF];
    output[pos++] = '\n';
    output[pos++] = '\n';

    // Scan for PRINT (0xB2) and PRINT_RET (0xB3) instructions near PC
    uint32_t texts_found = 0;
    const uint32_t MAX_TEXTS = 10;
    const uint32_t SCAN_RANGE = 1000;  // Scan 1000 bytes from PC

    for (uint32_t addr = pc; addr < pc + SCAN_RANGE && addr < GAME_SIZE - 10 && texts_found < MAX_TEXTS; addr++) {
        zbyte opcode = story_data[addr];

        // Check for PRINT or PRINT_RET
        if (opcode == 0xB2 || opcode == 0xB3) {
            uint32_t save_pos = pos;

            // Text follows immediately after opcode
            output_ptr = output + pos;
            output_limit = 3900 - pos;

            decode_text(addr + 1);

            uint32_t decoded_len = (output_ptr - (output + pos));
            if (decoded_len >= 3 && decoded_len < 300) {
                // Valid text!
                texts_found++;
                pos = output_ptr - output;
                output[pos++] = '\n';
                output[pos++] = '\n';
            } else {
                // Invalid, revert
                pos = save_pos;
            }
        }
    }

    msg = "--- Found ";
    while (*msg) output[pos++] = *msg++;
    if (texts_found >= 10) output[pos++] = '0' + (texts_found / 10);
    output[pos++] = '0' + (texts_found % 10);
    msg = " text strings! ---\n";
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
