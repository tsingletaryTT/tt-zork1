// Ultra-minimal object decoder - just 5 objects for speed!
#include <cstdint>

typedef uint8_t zbyte;
typedef uint16_t zword;

static zbyte* story_data;
static char* output;
static uint32_t pos;

static char get_char(int alph, int idx) {
    if (alph == 0 && idx >= 6 && idx <= 31) return 'a' + (idx - 6);
    if (alph == 1 && idx >= 6 && idx <= 31) return 'A' + (idx - 6);
    if (idx == 0) return ' ';
    return '?';
}

static void decode_obj(zword addr, uint32_t max_words) {
    int shift = 0;
    for (uint32_t w = 0; w < max_words && addr < 86000; w++) {
        zword word = (story_data[addr] << 8) | story_data[addr + 1];
        addr += 2;
        
        for (int s = 10; s >= 0; s -= 5) {
            zbyte c = (word >> s) & 0x1F;
            if (c >= 6) {
                output[pos++] = get_char(shift, c);
                shift = 0;
            } else if (c == 4 || c == 5) {
                shift = (c == 4) ? 1 : 2;
            } else if (c == 0) {
                output[pos++] = ' ';
                shift = 0;
            }
        }
        if (word & 0x8000) break;
    }
}

void kernel_main() {
    uint32_t game_dram = get_arg_val<uint32_t>(0);
    uint32_t out_dram = get_arg_val<uint32_t>(4);

    constexpr uint32_t L1_GAME = 0x10000;
    constexpr uint32_t L1_OUT = 0x50000;

    InterleavedAddrGen<true> gen = {.bank_base_address = game_dram, .page_size = 1024};
    
    for (uint32_t off = 0; off < 86838; off += 1024) {
        uint32_t sz = (off + 1024 <= 86838) ? 1024 : (86838 - off);
        noc_async_read(get_noc_addr(off / 1024, gen), L1_GAME + off, sz);
    }
    noc_async_read_barrier();

    story_data = (zbyte*)L1_GAME;
    output = (char*)L1_OUT;
    pos = 0;

    const char* h = "=== ZORK OBJECTS 1-70! ===\n(Looking for Object 64: West of House!)\n\n";
    while (*h) output[pos++] = *h++;

    zword obj_start = ((story_data[0x0A] << 8) | story_data[0x0B]) + 62;

    // Decode objects 1-70 to find West of House!
    for (uint32_t i = 1; i <= 70; i++) {
        zword entry = obj_start + ((i - 1) * 9);
        zword prop = (story_data[entry + 7] << 8) | story_data[entry + 8];
        
        if (prop > 0 && prop < 86000) {
            zbyte len = story_data[prop];
            if (len > 0 && len < 20) {
                // Display object number (handle 1-2 digits)
                if (i >= 10) output[pos++] = '0' + (i / 10);
                output[pos++] = '0' + (i % 10);
                output[pos++] = '.';
                output[pos++] = ' ';
                decode_obj(prop + 1, len);
                output[pos++] = '\n';
            }
        }
    }

    output[pos++] = '\0';

    InterleavedAddrGen<true> out_gen = {.bank_base_address = out_dram, .page_size = 4096};
    noc_async_write(L1_OUT, get_noc_addr(0, out_gen), 4096);
    noc_async_write_barrier();
}
