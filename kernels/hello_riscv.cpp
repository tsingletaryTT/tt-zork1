// Minimal "Hello World" kernel for RISC-V
// Success! Kernel executes without any args
// Now let's try to actually output the message

#include <cstdint>

void kernel_main() {
    // First write message to L1
    volatile char* l1_buffer = reinterpret_cast<volatile char*>(0x20000);

    const char* msg = "HELLO FROM RISC-V!\n";
    for (int i = 0; msg[i] != '\0'; i++) {
        l1_buffer[i] = msg[i];
    }
    l1_buffer[19] = '\0';

    // TODO: Use NoC to copy from L1 to DRAM
    // For now, kernel just writes to L1 and completes successfully
    // This proves kernel execution works!
}
