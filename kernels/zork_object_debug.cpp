// Debug object table structure
#include <cstdint>

typedef uint8_t zbyte;
typedef uint16_t zword;

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

    zbyte* story_data = (zbyte*)L1_GAME;
    char* output = (char*)L1_OUTPUT;
    uint32_t pos = 0;

    zword obj_table = (story_data[0x0A] << 8) | story_data[0x0B];

    const char* header = "=== OBJECT TABLE DEBUG ===\n\n";
    while (*header) output[pos++] = *header++;

    const char* msg = "Object table: 0x";
    while (*msg) output[pos++] = *msg++;
    const char* hex = "0123456789ABCDEF";
    output[pos++] = hex[(obj_table >> 12) & 0xF];
    output[pos++] = hex[(obj_table >> 8) & 0xF];
    output[pos++] = hex[(obj_table >> 4) & 0xF];
    output[pos++] = hex[obj_table & 0xF];
    output[pos++] = '\n';
    output[pos++] = '\n';

    // Skip 31 words (62 bytes) of defaults
    zword first_obj = obj_table + 62;

    msg = "First object entry at: 0x";
    while (*msg) output[pos++] = *msg++;
    output[pos++] = hex[(first_obj >> 12) & 0xF];
    output[pos++] = hex[(first_obj >> 8) & 0xF];
    output[pos++] = hex[(first_obj >> 4) & 0xF];
    output[pos++] = hex[first_obj & 0xF];
    output[pos++] = '\n';
    output[pos++] = '\n';

    // Show first 5 objects
    for (uint32_t i = 0; i < 5; i++) {
        zword obj_addr = first_obj + (i * 9);
        
        msg = "Object ";
        while (*msg) output[pos++] = *msg++;
        output[pos++] = '0' + (i + 1);
        msg = ":\n";
        while (*msg) output[pos++] = *msg++;

        // Property table pointer (bytes 7-8)
        zword prop_addr = (story_data[obj_addr + 7] << 8) | story_data[obj_addr + 8];
        
        msg = "  Prop table: 0x";
        while (*msg) output[pos++] = *msg++;
        output[pos++] = hex[(prop_addr >> 12) & 0xF];
        output[pos++] = hex[(prop_addr >> 8) & 0xF];
        output[pos++] = hex[(prop_addr >> 4) & 0xF];
        output[pos++] = hex[prop_addr & 0xF];
        output[pos++] = '\n';

        if (prop_addr > 0 && prop_addr < GAME_SIZE - 20) {
            zbyte text_len = story_data[prop_addr];
            msg = "  Text len: ";
            while (*msg) output[pos++] = *msg++;
            if (text_len >= 10) output[pos++] = '0' + (text_len / 10);
            output[pos++] = '0' + (text_len % 10);
            msg = " words\n";
            while (*msg) output[pos++] = *msg++;

            // Show first 16 bytes
            msg = "  Data: ";
            while (*msg) output[pos++] = *msg++;
            uint32_t data_limit = (uint32_t)(text_len * 2 + 10);
            for (uint32_t j = 0; j < 16 && j < data_limit; j++) {
                zbyte b = story_data[prop_addr + j];
                output[pos++] = hex[(b >> 4) & 0xF];
                output[pos++] = hex[b & 0xF];
                output[pos++] = ' ';
            }
            output[pos++] = '\n';
        }
        output[pos++] = '\n';
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
