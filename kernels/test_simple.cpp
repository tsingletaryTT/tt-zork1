// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * test_simple.cpp - Simple kernel that writes to output buffer
 *
 * No includes, no templates, just basic C++
 */

// TT-Metal kernel API - forward declaration
extern "C" {
    unsigned int get_arg_val_u32(unsigned int arg_index);
}

template<typename T>
inline T get_arg_val(unsigned int arg_index) {
    return (T)get_arg_val_u32(arg_index);
}

void kernel_main() {
    // Get output buffer address
    unsigned int output_addr = get_arg_val<unsigned int>(4);

    // Cast to char pointer
    char* output = (char*)output_addr;

    // Write simple message manually (no strcpy needed)
    output[0] = 'H';
    output[1] = 'E';
    output[2] = 'L';
    output[3] = 'L';
    output[4] = 'O';
    output[5] = ' ';
    output[6] = 'R';
    output[7] = 'I';
    output[8] = 'S';
    output[9] = 'C';
    output[10] = '-';
    output[11] = 'V';
    output[12] = '!';
    output[13] = '\n';
    output[14] = '\0';
}
