// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * test_zstring_simple.cpp - Test Z-string decoder with known input
 *
 * Create a simple Z-string manually and decode it to verify algorithm.
 *
 * Z-string encoding for "hello":
 * h=7+6=13, e=4+6=10, l=11+6=17, l=11+6=17, o=14+6=20
 *
 * Pack into 16-bit words (3 chars per word, 5 bits each):
 * Word 1: [end=1][13][10][17] = 1_01101_01010_10001 = 0xB551
 * Word 2: [end=1][17][20][pad=5] = 1_10001_10100_00101 = 0xC685
 *
 * Actually for a complete word:
 * "hello" needs 5 chars = 2 words (3 + 2 chars)
 * Word 1: [end=0][h=13][e=10][l=17] = 0_01101_01010_10001 = 0x3551
 * Word 2: [end=1][l=17][o=20][pad=5] = 1_10001_10100_00101 = 0xC685
 */

void kernel_main() {
    uint32_t output_dram = get_arg_val<uint32_t>(4);

    constexpr uint32_t L1_TEST_STRING = 0x10000;
    constexpr uint32_t L1_OUTPUT = 0x20000;
    constexpr uint32_t OUTPUT_SIZE = 1024;

    // Create test Z-string: "hello" (manually encoded)
    uint16_t* test_zstring = (uint16_t*)L1_TEST_STRING;

    // Encode "hello":
    // Alphabet A0 (lowercase): a=0, b=1, c=2, d=3, e=4, ... h=7, ... l=11, ... o=14
    // Z-char = alphabet_index + 6
    // h=7+6=13 (0x0D), e=4+6=10 (0x0A), l=11+6=17 (0x11), l=17, o=14+6=20 (0x14)

    // Word 1: bits [15-11]=13, [10-6]=10, [5-1]=17, [0]=more bit
    // 00000 01101 01010 10001 = 0x0000 | 0x1A00 | 0x0140 | 0x0011 = ...
    // Actually: [bit 15]=end, [bits 14-10]=char0, [bits 9-5]=char1, [bits 4-0]=char2
    // Word 1: [0][01101][01010][10001] = 0x3551
    test_zstring[0] = 0x3551;

    // Word 2: [end=1][l=17][o=20][pad=5]
    // [1][10001][10100][00101] = 0xC685
    test_zstring[1] = 0xC685;

    // Now decode it
    char* output = (char*)L1_OUTPUT;
    uint32_t pos = 0;

    const char* header = "=== Z-STRING DECODER TEST ===\n\nTest string: \"hello\"\n";
    while (*header) output[pos++] = *header++;

    // Show hex encoding
    const char* msg1 = "Encoded as: 0x";
    while (*msg1) output[pos++] = *msg1++;

    const char* hex = "0123456789ABCDEF";
    uint16_t word1 = test_zstring[0];
    output[pos++] = hex[(word1 >> 12) & 0xF];
    output[pos++] = hex[(word1 >> 8) & 0xF];
    output[pos++] = hex[(word1 >> 4) & 0xF];
    output[pos++] = hex[word1 & 0xF];
    output[pos++] = ' ';
    output[pos++] = '0';
    output[pos++] = 'x';
    uint16_t word2 = test_zstring[1];
    output[pos++] = hex[(word2 >> 12) & 0xF];
    output[pos++] = hex[(word2 >> 8) & 0xF];
    output[pos++] = hex[(word2 >> 4) & 0xF];
    output[pos++] = hex[word2 & 0xF];
    output[pos++] = '\n';
    output[pos++] = '\n';

    // Decode character by character
    const char* msg2 = "Decoding:\n";
    while (*msg2) output[pos++] = *msg2++;

    uint8_t* zstring_bytes = (uint8_t*)L1_TEST_STRING;
    int shift_state = 0;

    // Process first word
    for (int word_idx = 0; word_idx < 2; word_idx++) {
        uint16_t word = test_zstring[word_idx];

        for (int i = 10; i >= 0; i -= 5) {
            uint8_t c = (word >> i) & 0x1F;

            // Debug output
            const char* prefix = "  char=";
            while (*prefix) output[pos++] = *prefix++;
            output[pos++] = '0' + (c / 10);
            output[pos++] = '0' + (c % 10);
            output[pos++] = ' ';
            output[pos++] = '(';
            output[pos++] = '0';
            output[pos++] = 'x';
            output[pos++] = hex[c >> 4];
            output[pos++] = hex[c & 0xF];
            output[pos++] = ')';
            output[pos++] = ' ';
            output[pos++] = '=';
            output[pos++] = '>';
            output[pos++] = ' ';

            if (c == 0) {
                output[pos++] = '[';
                output[pos++] = 'S';
                output[pos++] = 'P';
                output[pos++] = 'A';
                output[pos++] = 'C';
                output[pos++] = 'E';
                output[pos++] = ']';
            } else if (c >= 6 && c <= 31) {
                // Regular character from alphabet A0 (lowercase)
                char ch = 'a' + (c - 6);
                output[pos++] = '\'';
                output[pos++] = ch;
                output[pos++] = '\'';
            } else if (c == 5) {
                output[pos++] = '[';
                output[pos++] = 'P';
                output[pos++] = 'A';
                output[pos++] = 'D';
                output[pos++] = ']';
            } else {
                output[pos++] = '[';
                output[pos++] = '?';
                output[pos++] = ']';
            }

            output[pos++] = '\n';
        }
    }

    output[pos++] = '\n';
    const char* footer = "Expected: 'h' 'e' 'l' 'l' 'o'\n";
    while (*footer) output[pos++] = *footer++;
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
