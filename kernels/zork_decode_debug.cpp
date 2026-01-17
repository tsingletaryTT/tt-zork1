// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_decode_debug.cpp - Debug object table parsing
 *
 * Let's see what's actually in the object table and property addresses.
 */

uint16_t read_word(uint8_t* mem, uint16_t addr) {
    return (mem[addr] << 8) | mem[addr + 1];
}

void kernel_main() {
    uint32_t game_data_dram = get_arg_val<uint32_t>(0);
    uint32_t output_dram = get_arg_val<uint32_t>(4);

    constexpr uint32_t L1_GAME = 0x10000;
    constexpr uint32_t L1_OUTPUT = 0x50000;
    constexpr uint32_t GAME_SIZE = 86838;
    constexpr uint32_t OUTPUT_SIZE = 4096;

    // Read game
    InterleavedAddrGen<true> game_gen = {
        .bank_base_address = game_data_dram,
        .page_size = GAME_SIZE
    };
    uint64_t game_noc = get_noc_addr(0, game_gen);
    noc_async_read(game_noc, L1_GAME, GAME_SIZE);
    noc_async_read_barrier();

    uint8_t* mem = (uint8_t*)L1_GAME;
    char* output = (char*)L1_OUTPUT;
    uint32_t pos = 0;

    const char* hex = "0123456789ABCDEF";

    auto write_str = [&](const char* str) {
        while (*str) output[pos++] = *str++;
    };

    auto write_hex_word = [&](uint16_t val) {
        output[pos++] = '0';
        output[pos++] = 'x';
        output[pos++] = hex[(val >> 12) & 0xF];
        output[pos++] = hex[(val >> 8) & 0xF];
        output[pos++] = hex[(val >> 4) & 0xF];
        output[pos++] = hex[val & 0xF];
    };

    auto write_dec = [&](uint32_t val) {
        if (val >= 100) output[pos++] = '0' + (val / 100);
        if (val >= 10) output[pos++] = '0' + ((val % 100) / 10);
        output[pos++] = '0' + (val % 10);
    };

    write_str("=== OBJECT TABLE DEBUG ===\n\n");

    // Read header
    uint8_t version = mem[0];
    write_str("Version: ");
    write_dec(version);
    write_str("\n");

    uint16_t obj_table = read_word(mem, 0x0A);
    write_str("Object table addr: ");
    write_hex_word(obj_table);
    write_str("\n");

    uint16_t first_obj = obj_table + 62;
    write_str("First object addr: ");
    write_hex_word(first_obj);
    write_str("\n\n");

    // Show first 5 objects
    for (uint32_t i = 1; i <= 5; i++) {
        write_str("Object ");
        write_dec(i);
        write_str(":\n");

        uint16_t obj_addr = first_obj + ((i - 1) * 9);
        write_str("  Obj addr: ");
        write_hex_word(obj_addr);
        write_str("\n");

        // Read object entry (9 bytes)
        uint16_t prop_addr = read_word(mem, obj_addr + 7);
        write_str("  Prop addr: ");
        write_hex_word(prop_addr);
        write_str("\n");

        if (prop_addr != 0 && prop_addr < GAME_SIZE - 20) {
            // Read property table
            uint8_t name_len = mem[prop_addr];
            write_str("  Name len (words): ");
            write_dec(name_len);
            write_str("\n");

            if (name_len > 0 && name_len <= 10) {
                write_str("  Z-string bytes: ");
                for (uint32_t j = 0; j < name_len * 2 && j < 10; j++) {
                    uint8_t byte = mem[prop_addr + 1 + j];
                    output[pos++] = hex[byte >> 4];
                    output[pos++] = hex[byte & 0xF];
                    output[pos++] = ' ';
                }
                write_str("\n");
            }
        } else {
            write_str("  [Invalid prop addr]\n");
        }
        write_str("\n");
    }

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
