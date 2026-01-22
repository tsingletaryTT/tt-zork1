// Ultra-minimal test: just write "HELLO" to output buffer
#include <cstdint>

void kernel_main() {
    // Use the EXACT addresses that the host allocated
    constexpr uint32_t L1_OUT = 0x17f400;

    volatile char* output = reinterpret_cast<volatile char*>(L1_OUT);

    output[0] = 'H';
    output[1] = 'E';
    output[2] = 'L';
    output[3] = 'L';
    output[4] = 'O';
    output[5] = ' ';
    output[6] = 'F';
    output[7] = 'R';
    output[8] = 'O';
    output[9] = 'M';
    output[10] = ' ';
    output[11] = 'R';
    output[12] = 'I';
    output[13] = 'S';
    output[14] = 'C';
    output[15] = '-';
    output[16] = 'V';
    output[17] = '!';
    output[18] = '\n';
    output[19] = '\0';
}
