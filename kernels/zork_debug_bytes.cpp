// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_debug_bytes.cpp - Show raw bytes of object entries
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

    write_str("=== RAW BYTES DEBUG ===\n\n");

    // Object table location
    uint16_t obj_table = read_word(mem, 0x0A);
    uint16_t first_obj = obj_table + 62;

    write_str("Object table: 0x");
    output[pos++] = hex[(obj_table >> 12) & 0xF];
    output[pos++] = hex[(obj_table >> 8) & 0xF];
    output[pos++] = hex[(obj_table >> 4) & 0xF];
    output[pos++] = hex[obj_table & 0xF];
    write_str("\n");

    write_str("First object: 0x");
    output[pos++] = hex[(first_obj >> 12) & 0xF];
    output[pos++] = hex[(first_obj >> 8) & 0xF];
    output[pos++] = hex[(first_obj >> 4) & 0xF];
    output[pos++] = hex[first_obj & 0xF];
    write_str("\n\n");

    // Show raw 9 bytes of first object
    write_str("Object 1 (9 bytes at 0x");
    output[pos++] = hex[(first_obj >> 12) & 0xF];
    output[pos++] = hex[(first_obj >> 8) & 0xF];
    output[pos++] = hex[(first_obj >> 4) & 0xF];
    output[pos++] = hex[first_obj & 0xF];
    write_str("):\n");

    for (uint32_t i = 0; i < 9; i++) {
        uint8_t byte = mem[first_obj + i];
        output[pos++] = hex[byte >> 4];
        output[pos++] = hex[byte & 0xF];
        output[pos++] = ' ';
    }
    write_str("\n\n");

    write_str("Interpretation:\n");
    write_str("  Bytes 0-3 (attributes): ");
    for (uint32_t i = 0; i < 4; i++) {
        output[pos++] = hex[mem[first_obj + i] >> 4];
        output[pos++] = hex[mem[first_obj + i] & 0xF];
        output[pos++] = ' ';
    }
    write_str("\n");

    write_str("  Byte 4 (parent): ");
    output[pos++] = hex[mem[first_obj + 4] >> 4];
    output[pos++] = hex[mem[first_obj + 4] & 0xF];
    write_str("\n");

    write_str("  Byte 5 (sibling): ");
    output[pos++] = hex[mem[first_obj + 5] >> 4];
    output[pos++] = hex[mem[first_obj + 5] & 0xF];
    write_str("\n");

    write_str("  Byte 6 (child): ");
    output[pos++] = hex[mem[first_obj + 6] >> 4];
    output[pos++] = hex[mem[first_obj + 6] & 0xF];
    write_str("\n");

    write_str("  Bytes 7-8 (prop addr): ");
    uint16_t prop_addr = read_word(mem, first_obj + 7);
    output[pos++] = hex[(prop_addr >> 12) & 0xF];
    output[pos++] = hex[(prop_addr >> 8) & 0xF];
    output[pos++] = hex[(prop_addr >> 4) & 0xF];
    output[pos++] = hex[prop_addr & 0xF];
    write_str(" = ");
    // Decimal
    uint32_t val = prop_addr;
    if (val >= 10000) output[pos++] = '0' + (val / 10000);
    if (val >= 1000) output[pos++] = '0' + ((val % 10000) / 1000);
    if (val >= 100) output[pos++] = '0' + ((val % 1000) / 100);
    if (val >= 10) output[pos++] = '0' + ((val % 100) / 10);
    output[pos++] = '0' + (val % 10);
    write_str("\n\n");

    // Check if prop_addr is valid
    if (prop_addr < GAME_SIZE - 20) {
        write_str("Property table at 0x");
        output[pos++] = hex[(prop_addr >> 12) & 0xF];
        output[pos++] = hex[(prop_addr >> 8) & 0xF];
        output[pos++] = hex[(prop_addr >> 4) & 0xF];
        output[pos++] = hex[prop_addr & 0xF];
        write_str(":\n");

        write_str("  text-length: ");
        uint8_t text_len = mem[prop_addr];
        output[pos++] = hex[text_len >> 4];
        output[pos++] = hex[text_len & 0xF];
        write_str(" = ");
        output[pos++] = '0' + (text_len / 10);
        output[pos++] = '0' + (text_len % 10);
        write_str(" words (");
        uint32_t bytes = text_len * 2;
        if (bytes >= 100) output[pos++] = '0' + (bytes / 100);
        if (bytes >= 10) output[pos++] = '0' + ((bytes % 100) / 10);
        output[pos++] = '0' + (bytes % 10);
        write_str(" bytes)\n");

        write_str("  First 10 text bytes: ");
        for (uint32_t i = 0; i < 10 && i < text_len * 2; i++) {
            uint8_t byte = mem[prop_addr + 1 + i];
            output[pos++] = hex[byte >> 4];
            output[pos++] = hex[byte & 0xF];
            output[pos++] = ' ';
        }
        write_str("\n");
    } else {
        write_str("[Property address out of range!]\n");
    }

    output[pos++] = '\0';

    InterleavedAddrGen<true> out_gen = {
        .bank_base_address = output_dram,
        .page_size = OUTPUT_SIZE
    };
    uint64_t out_noc = get_noc_addr(0, out_gen);
    noc_async_write(L1_OUTPUT, out_noc, OUTPUT_SIZE);
    noc_async_write_barrier();
}
