// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * test_hello.cpp - Minimal test kernel for Blackhole
 *
 * This is a minimal test to prove:
 * 1. We can load a kernel on Blackhole
 * 2. Kernel can read from DRAM buffer (game data)
 * 3. Kernel can write to DRAM buffer (output)
 * 4. Host can read the output
 *
 * If this works, we know the infrastructure is solid!
 */

#include <cstdint>

// TT-Metal kernel API
template<typename T>
inline T get_arg_val(uint32_t arg_index);

/**
 * Simple strlen implementation
 */
uint32_t simple_strlen(const char* str) {
    uint32_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

/**
 * Simple string copy
 */
void simple_strcpy(char* dest, const char* src) {
    while (*src != '\0') {
        *dest++ = *src++;
    }
    *dest = '\0';
}

/**
 * Kernel entry point
 */
void kernel_main() {
    // Get buffer addresses from runtime arguments
    uint32_t game_data_addr = get_arg_val<uint32_t>(0);
    uint32_t game_data_size = get_arg_val<uint32_t>(1);
    uint32_t input_addr     = get_arg_val<uint32_t>(2);
    uint32_t input_size     = get_arg_val<uint32_t>(3);
    uint32_t output_addr    = get_arg_val<uint32_t>(4);
    uint32_t output_size    = get_arg_val<uint32_t>(5);

    // Cast to usable pointers
    const uint8_t* game_data = (const uint8_t*)game_data_addr;
    const char* input = (const char*)input_addr;
    char* output = (char*)output_addr;

    // Write greeting to output buffer
    const char* greeting = "🎉 HELLO FROM BLACKHOLE RISC-V! 🎉\n\n";
    simple_strcpy(output, greeting);
    output += simple_strlen(greeting);

    // Report buffer info
    const char* info1 = "Kernel is running on Blackhole RISC-V core!\n";
    simple_strcpy(output, info1);
    output += simple_strlen(info1);

    // Report game data info
    const char* info2 = "Game data received: ";
    simple_strcpy(output, info2);
    output += simple_strlen(info2);

    // Convert size to string (simple version)
    char size_str[32];
    uint32_t size = game_data_size;
    int i = 0;
    if (size == 0) {
        size_str[i++] = '0';
    } else {
        char temp[32];
        int j = 0;
        while (size > 0) {
            temp[j++] = '0' + (size % 10);
            size /= 10;
        }
        // Reverse
        while (j > 0) {
            size_str[i++] = temp[--j];
        }
    }
    size_str[i] = '\0';
    simple_strcpy(output, size_str);
    output += i;

    const char* info3 = " bytes\n";
    simple_strcpy(output, info3);
    output += simple_strlen(info3);

    // Read first few bytes of game data
    const char* info4 = "First 4 bytes of game data: ";
    simple_strcpy(output, info4);
    output += simple_strlen(info4);

    // Show as hex
    for (int i = 0; i < 4 && i < (int)game_data_size; i++) {
        uint8_t byte = game_data[i];
        char hex[4] = "0x";
        hex[2] = "0123456789ABCDEF"[byte >> 4];
        hex[3] = "0123456789ABCDEF"[byte & 0xF];
        for (int j = 0; j < 4; j++) {
            *output++ = hex[j];
        }
        *output++ = ' ';
    }
    *output++ = '\n';

    // Echo input
    const char* info5 = "\nInput received: ";
    simple_strcpy(output, info5);
    output += simple_strlen(info5);
    simple_strcpy(output, input);
    output += simple_strlen(input);

    const char* final = "\n\n✓ Blackhole RISC-V kernel test SUCCESSFUL!\n";
    simple_strcpy(output, final);

    // Kernel done - return to host
}
