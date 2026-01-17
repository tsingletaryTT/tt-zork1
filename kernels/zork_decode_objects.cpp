// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_decode_objects.cpp - Decode object names from Z-machine
 *
 * Objects in Z-machine v3 have short text descriptions (Z-strings).
 * By decoding these, we can see actual Zork text like "mailbox", "leaflet", etc.
 *
 * Object table structure (v3):
 * - Header 0x0A-0x0B: Address of object table
 * - Object table starts with 31 default property values (62 bytes)
 * - Then objects, each is 9 bytes:
 *   [0-3]: Attributes (4 bytes)
 *   [4]: Parent object number
 *   [5]: Sibling object number
 *   [6]: Child object number
 *   [7-8]: Property table address
 * - Property table for each object:
 *   [0-1]: Length of short name (in words)
 *   [2...]: Z-string of short name
 *   [after string]: Properties...
 */

struct ZMachineState {
    uint8_t* memory;
    char* output_buffer;
    uint32_t output_pos;
};

// Read 16-bit word (big-endian)
uint16_t read_word(uint8_t* mem, uint16_t addr) {
    return (mem[addr] << 8) | mem[addr + 1];
}

// Decode Z-string (compressed text format)
// Returns: number of bytes consumed
uint16_t decode_zstring(ZMachineState* zm, uint16_t addr) {
    uint16_t pos = addr;
    uint8_t current_alphabet = 0;  // 0=lowercase, 1=uppercase, 2=punctuation

    // Read words until we hit the end bit
    while (pos < 65000 && zm->output_pos < 3800) {
        // Read 2-byte word (big-endian)
        uint16_t word = read_word(zm->memory, pos);
        pos += 2;

        // Extract 3 characters (5 bits each)
        uint8_t c0 = (word >> 10) & 0x1F;
        uint8_t c1 = (word >> 5) & 0x1F;
        uint8_t c2 = word & 0x1F;

        // Decode each character
        for (uint32_t i = 0; i < 3; i++) {
            uint8_t c = (i == 0) ? c0 : (i == 1) ? c1 : c2;

            if (c == 0) {
                // Space
                zm->output_buffer[zm->output_pos++] = ' ';
                current_alphabet = 0;

            } else if (c >= 1 && c <= 3) {
                // Abbreviation or shift - skip for now
                current_alphabet = (c == 2) ? 1 : (c == 3) ? 2 : current_alphabet;

            } else if (c == 4) {
                // Shift lock to uppercase
                current_alphabet = 1;

            } else if (c == 5) {
                // Shift lock to punctuation
                current_alphabet = 2;

            } else if (c >= 6 && c <= 31) {
                // Regular character
                char ch;
                if (current_alphabet == 0) {
                    // Lowercase a-z
                    ch = 'a' + (c - 6);
                } else if (current_alphabet == 1) {
                    // Uppercase A-Z
                    ch = 'A' + (c - 6);
                } else {
                    // Punctuation - simplified mapping
                    if (c == 6) ch = ' ';
                    else if (c == 7) ch = '\n';
                    else if (c >= 8 && c <= 17) ch = '0' + (c - 8);
                    else if (c == 18) ch = '.';
                    else if (c == 19) ch = ',';
                    else if (c == 20) ch = '!';
                    else if (c == 21) ch = '?';
                    else if (c == 22) ch = '_';
                    else if (c == 23) ch = '#';
                    else if (c == 24) ch = '\'';
                    else if (c == 25) ch = '"';
                    else if (c == 26) ch = '/';
                    else if (c == 27) ch = '\\';
                    else if (c == 28) ch = '-';
                    else if (c == 29) ch = ':';
                    else if (c == 30) ch = '(';
                    else ch = ')';
                }

                zm->output_buffer[zm->output_pos++] = ch;
                current_alphabet = 0;  // Reset after character
            }
        }

        // Check end bit (bit 15)
        if (word & 0x8000) {
            break;  // Last word
        }
    }

    return pos - addr;
}

void kernel_main() {
    uint32_t game_data_dram = get_arg_val<uint32_t>(0);
    uint32_t output_dram = get_arg_val<uint32_t>(4);

    // L1 memory layout
    constexpr uint32_t L1_GAME_MEMORY = 0x10000;
    constexpr uint32_t L1_OUTPUT = 0x50000;
    constexpr uint32_t GAME_READ_SIZE = 86838;  // Full game
    constexpr uint32_t OUTPUT_SIZE = 4096;

    // Read entire game into L1
    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = game_data_dram,
        .page_size = GAME_READ_SIZE
    };
    uint64_t game_noc = get_noc_addr(0, game_gen);
    noc_async_read(game_noc, L1_GAME_MEMORY, GAME_READ_SIZE);
    noc_async_read_barrier();

    // Initialize state
    ZMachineState zm;
    zm.memory = (uint8_t*)L1_GAME_MEMORY;
    zm.output_buffer = (char*)L1_OUTPUT;
    zm.output_pos = 0;

    // Write header
    const char* header = "=== ZORK OBJECT NAMES FROM BLACKHOLE! ===\n\n";
    while (*header) {
        zm.output_buffer[zm.output_pos++] = *header++;
    }

    // Parse header
    uint16_t obj_table_addr = read_word(zm.memory, 0x0A);  // Offset 0x0A = object table

    // Skip 31 default properties (62 bytes)
    uint16_t first_obj_addr = obj_table_addr + 62;

    // Decode first 10 objects
    const uint32_t NUM_OBJECTS = 10;
    for (uint32_t obj_num = 1; obj_num <= NUM_OBJECTS; obj_num++) {
        // Each object is 9 bytes
        uint16_t obj_addr = first_obj_addr + ((obj_num - 1) * 9);

        // Get property table address (bytes 7-8 of object entry)
        uint16_t prop_addr = read_word(zm.memory, obj_addr + 7);

        if (prop_addr == 0 || prop_addr >= GAME_READ_SIZE - 10) {
            continue;  // Invalid
        }

        // Property table starts with short name length (in words)
        uint8_t name_len_words = zm.memory[prop_addr];

        if (name_len_words == 0 || name_len_words > 20) {
            continue;  // No name or invalid
        }

        // Write object number
        const char* prefix = "Object ";
        while (*prefix) zm.output_buffer[zm.output_pos++] = *prefix++;
        zm.output_buffer[zm.output_pos++] = '0' + (obj_num / 10);
        zm.output_buffer[zm.output_pos++] = '0' + (obj_num % 10);
        zm.output_buffer[zm.output_pos++] = ':';
        zm.output_buffer[zm.output_pos++] = ' ';
        zm.output_buffer[zm.output_pos++] = '\"';

        // Decode short name (starts at prop_addr + 1)
        decode_zstring(&zm, prop_addr + 1);

        zm.output_buffer[zm.output_pos++] = '\"';
        zm.output_buffer[zm.output_pos++] = '\n';
    }

    zm.output_buffer[zm.output_pos++] = '\n';
    const char* footer = "--- Decoded 10 object names! ---\n";
    while (*footer) zm.output_buffer[zm.output_pos++] = *footer++;

    zm.output_buffer[zm.output_pos++] = '\0';

    // Write output to DRAM
    InterleavedAddrGen<true> out_gen = {
        .bank_base_address = output_dram,
        .page_size = OUTPUT_SIZE
    };
    uint64_t out_noc = get_noc_addr(0, out_gen);
    noc_async_write(L1_OUTPUT, out_noc, OUTPUT_SIZE);
    noc_async_write_barrier();
}
