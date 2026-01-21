// Minimal "Hello World" kernel for RISC-V
// Just write a simple pattern to output buffer and exit
//
// WORKAROUND: get_arg_val() and get_common_arg_val() cause kernel execution to hang
// when using MeshWorkload APIs on TT-Metal. Using hardcoded L1 address 0x1000 instead.

#include <cstdint>

void kernel_main() {
    // Read runtime args from hardcoded L1 address (workaround for MeshWorkload issue)
    volatile uint32_t* args = reinterpret_cast<volatile uint32_t*>(0x1000);
    uint32_t output_addr = args[0];

    // Write simple test pattern to output
    volatile char* output = reinterpret_cast<volatile char*>(output_addr);

    const char* msg = "HELLO FROM RISC-V!\n";
    for (int i = 0; msg[i] != '\0'; i++) {
        output[i] = msg[i];
    }
    output[19] = '\0';  // Null terminator

    // Exit successfully
}
