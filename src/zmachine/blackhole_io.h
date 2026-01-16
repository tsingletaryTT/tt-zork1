/*
 * blackhole_io.h - Blackhole-specific I/O layer for Frotz Z-machine
 *
 * This file provides the I/O abstraction layer for running Frotz on
 * Tenstorrent Blackhole RISC-V cores. It replaces file I/O and terminal
 * I/O with DRAM buffer operations.
 *
 * Architecture:
 * - Host program loads game file into DRAM buffer
 * - Kernel receives buffer pointers as runtime arguments
 * - Game data read from DRAM buffer (not filesystem)
 * - Input commands read from DRAM input buffer
 * - Output text written to DRAM output buffer
 *
 * Memory Layout:
 * - Game data buffer: Up to 128KB (zork1.z3 is 86KB)
 * - Input buffer: 1KB for user commands
 * - Output buffer: 16KB for game text
 *
 * Usage:
 * 1. Host sets up buffers and passes pointers via SetRuntimeArgs()
 * 2. Kernel calls blackhole_io_init() with buffer pointers
 * 3. Z-machine reads game data via blackhole_read_game_data()
 * 4. Z-machine reads input via blackhole_read_line()
 * 5. Z-machine writes output via blackhole_write_string()
 */

#ifndef BLACKHOLE_IO_H
#define BLACKHOLE_IO_H

#include <stdint.h>
#include <stddef.h>

/* Buffer state structure */
typedef struct {
    /* Game data buffer (read-only, set at init) */
    const uint8_t *game_data;
    uint32_t game_data_size;
    uint32_t game_data_pos;  /* Current read position */

    /* Input buffer (read from host) */
    const char *input_buffer;
    uint32_t input_buffer_size;
    uint32_t input_read_pos;

    /* Output buffer (write to host) */
    char *output_buffer;
    uint32_t output_buffer_size;
    uint32_t output_write_pos;
} blackhole_io_state_t;

/* Global I/O state (accessed by all I/O functions) */
extern blackhole_io_state_t g_blackhole_io;

/**
 * Initialize Blackhole I/O system with DRAM buffer pointers
 *
 * Called at kernel startup with pointers received from host via runtime args.
 *
 * @param game_data_addr - Address of game file data in DRAM
 * @param game_data_size - Size of game file (86KB for zork1.z3)
 * @param input_addr - Address of input buffer in DRAM
 * @param input_size - Size of input buffer (1KB)
 * @param output_addr - Address of output buffer in DRAM
 * @param output_size - Size of output buffer (16KB)
 */
void blackhole_io_init(
    uint32_t game_data_addr,
    uint32_t game_data_size,
    uint32_t input_addr,
    uint32_t input_size,
    uint32_t output_addr,
    uint32_t output_size
);

/**
 * Read game data from DRAM buffer
 *
 * Replacement for fread() when reading Z-machine game file.
 * Reads from the game_data buffer initialized at startup.
 *
 * @param dest - Destination buffer
 * @param size - Size of each element
 * @param count - Number of elements to read
 * @return Number of elements actually read
 */
size_t blackhole_read_game_data(void *dest, size_t size, size_t count);

/**
 * Seek within game data buffer
 *
 * Replacement for fseek() when accessing Z-machine game file.
 *
 * @param offset - Byte offset
 * @param whence - SEEK_SET, SEEK_CUR, or SEEK_END
 * @return 0 on success, -1 on error
 */
int blackhole_seek_game_data(long offset, int whence);

/**
 * Get current position in game data buffer
 *
 * Replacement for ftell() when accessing Z-machine game file.
 *
 * @return Current byte offset in game data buffer
 */
long blackhole_tell_game_data(void);

/**
 * Read a line of input from DRAM input buffer
 *
 * Replacement for fgets() / terminal input.
 * Reads user command from the input buffer set by host.
 *
 * @param dest - Destination buffer for command
 * @param max_len - Maximum length to read (including null terminator)
 * @return dest on success, NULL on error or no input available
 */
char *blackhole_read_line(char *dest, size_t max_len);

/**
 * Write a string to DRAM output buffer
 *
 * Replacement for printf() / terminal output.
 * Writes game text to the output buffer for host to display.
 *
 * @param str - String to write
 * @return Number of characters written, or -1 on error
 */
int blackhole_write_string(const char *str);

/**
 * Flush output buffer
 *
 * In DRAM buffer model, this is essentially a no-op since writes
 * are immediately visible to host. Provided for API compatibility.
 */
void blackhole_flush_output(void);

#endif /* BLACKHOLE_IO_H */
