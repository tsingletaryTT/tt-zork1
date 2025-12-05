/**
 * io.h - I/O Abstraction Layer for Zork on Tenstorrent
 *
 * This file defines a platform-agnostic I/O interface that can be implemented
 * for different environments (native stdio, TT-Metal host interface, etc.)
 *
 * The abstraction allows us to:
 * - Develop and test on local machines
 * - Deploy to Tenstorrent hardware seamlessly
 * - Potentially add other backends (network, file replay, etc.)
 */

#ifndef TT_ZORK_IO_H
#define TT_ZORK_IO_H

#include <stdio.h>
#include <stdint.h>

/* Don't include stdbool.h - Frotz defines its own bool type */
#ifndef bool
typedef int bool;
#define true 1
#define false 0
#endif

/**
 * Initialize the I/O system
 * Returns: 0 on success, -1 on error
 */
int io_init(void);

/**
 * Shutdown the I/O system
 */
void io_shutdown(void);

/**
 * Print a single character to output
 */
void io_putchar(int c);

/**
 * Print a null-terminated string to output
 */
void io_print(const char *str);

/**
 * Print formatted output (like printf)
 */
void io_printf(const char *fmt, ...);

/**
 * Read a single character from input (blocking)
 * Returns: character read, or EOF on error/end-of-input
 */
int io_getchar(void);

/**
 * Read a line of input into buffer (up to max_len-1 chars + null terminator)
 * Returns: number of characters read (excluding newline), or -1 on error
 */
int io_getline(char *buffer, int max_len);

/**
 * Check if input is available (non-blocking)
 * Returns: true if input can be read without blocking
 */
bool io_input_ready(void);

/**
 * Flush output buffers
 */
void io_flush(void);

/**
 * Open a file for reading
 * Returns: FILE pointer on success, NULL on error
 */
FILE *io_fopen_read(const char *filename);

/**
 * Open a file for writing
 * Returns: FILE pointer on success, NULL on error
 */
FILE *io_fopen_write(const char *filename);

/**
 * Close a file
 */
void io_fclose(FILE *fp);

/**
 * Fatal error - print message and exit
 */
void io_fatal(const char *fmt, ...) __attribute__((noreturn));

/**
 * Warning - print message and continue
 */
void io_warn(const char *fmt, ...);

#endif /* TT_ZORK_IO_H */
