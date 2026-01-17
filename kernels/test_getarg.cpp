// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * test_getarg.cpp - Test get_arg_val function
 */

void kernel_main() {
    // Just call get_arg_val, don't do anything with it
    uint32_t addr = get_arg_val<uint32_t>(0);

    // Do nothing with addr, just return
    (void)addr;  // Prevent unused variable warning
}
