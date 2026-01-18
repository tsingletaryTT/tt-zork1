// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_object_names.cpp - Decode object names from property tables!
 *
 * RESEARCH BREAKTHROUGH: Room descriptions are stored as object names!
 * - Object table at header offset $0A
 * - Each object has property table with short name (Z-string)
 * - "West of House", "ZORK I" etc. are object names!
 *
 * References:
 * - http://inform-fiction.org/zmachine/standards/z1point0/sect12.html
 * - https://zspec.jaredreisinger.com/12-objects
 */

#include <cstdint>

typedef uint8_t zbyte;
typedef uint16_t zword;
typedef uint16_t zchar;

struct {
    zword abbreviations;
    zbyte version;
    zword object_table;  // NEW: Object table address from header $0A
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

// Real Frotz decoder
static void decode_text(zword z_addr) {
    zbyte c, prev_c = 0;
    int shift_state = 0;
    int shift_lock = 0;
    int status = 0;
    zword addr = z_addr;
    zword code;

    do {
        if (addr >= 86000) break;
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
                            decode_text(abbr_addr);
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

    // Initialize header
    z_header.version = story_data[0];
    z_header.abbreviations = (story_data[0x18] << 8) | story_data[0x19];
    z_header.object_table = (story_data[0x0A] << 8) | story_data[0x0B];  // Object table!

    const char* header = "=== ZORK OBJECT NAMES! ===\n\n";
    while (*header) output_buffer[output_pos++] = *header++;

    const char* msg = "Object table at: 0x";
    while (*msg) output_buffer[output_pos++] = *msg++;
    const char* hex = "0123456789ABCDEF";
    output_buffer[output_pos++] = hex[(z_header.object_table >> 12) & 0xF];
    output_buffer[output_pos++] = hex[(z_header.object_table >> 8) & 0xF];
    output_buffer[output_pos++] = hex[(z_header.object_table >> 4) & 0xF];
    output_buffer[output_pos++] = hex[z_header.object_table & 0xF];
    output_buffer[output_pos++] = '\n';
    output_buffer[output_pos++] = '\n';

    // Object table structure (v3):
    // - 31 words of property defaults (62 bytes)
    // - Then object entries: 9 bytes each
    //   - 4 bytes: attributes
    //   - 1 byte: parent
    //   - 1 byte: sibling
    //   - 1 byte: child
    //   - 2 bytes: property table address

    zword obj_defaults_end = z_header.object_table + 62;  // Skip 31 words
    zword obj_entry = obj_defaults_end;

    msg = "Decoding first 50 object names:\n\n";
    while (*msg) output_buffer[output_pos++] = *msg++;

    uint32_t objects_decoded = 0;
    const uint32_t MAX_OBJECTS = 50;

    for (uint32_t obj_num = 1; obj_num <= MAX_OBJECTS; obj_num++) {
        // Each object entry is 9 bytes
        zword entry_addr = obj_entry + ((obj_num - 1) * 9);

        if (entry_addr + 9 >= GAME_SIZE) break;

        // Property table address at offset +7 (2 bytes)
        zword prop_table_addr = (story_data[entry_addr + 7] << 8) | story_data[entry_addr + 8];

        if (prop_table_addr == 0 || prop_table_addr >= GAME_SIZE - 10) continue;

        // Property table format:
        // - Byte 0: text-length (number of 2-byte words)
        // - Bytes 1+: Z-string short name
        zbyte text_len = story_data[prop_table_addr];

        if (text_len > 0 && text_len < 20) {  // Sanity check
            zword name_addr = prop_table_addr + 1;

            uint32_t save_pos = output_pos;

            // Object number
            if (obj_num >= 10) output_buffer[output_pos++] = '0' + (obj_num / 10);
            output_buffer[output_pos++] = '0' + (obj_num % 10);
            msg = ". ";
            while (*msg) output_buffer[output_pos++] = *msg++;

            // Decode name
            decode_text(name_addr);

            uint32_t decoded_len = output_pos - save_pos;
            if (decoded_len > 3 && decoded_len < 100) {
                // Valid!
                objects_decoded++;
                output_buffer[output_pos++] = '\n';
            } else {
                // Revert
                output_pos = save_pos;
            }
        }
    }

    msg = "\n--- Decoded ";
    while (*msg) output_buffer[output_pos++] = *msg++;
    if (objects_decoded >= 10) output_buffer[output_pos++] = '0' + (objects_decoded / 10);
    output_buffer[output_pos++] = '0' + (objects_decoded % 10);
    msg = " object names! ---\n";
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
