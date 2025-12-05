/**
 * io_native.c - Native I/O Implementation using Standard C Library
 *
 * This implementation uses stdio for input/output, suitable for:
 * - Local development and testing on macOS/Linux/Windows
 * - Quick iteration without hardware
 */

#include "io.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

int io_init(void) {
    /* For native implementation, stdio is already initialized */
    /* Set stdout to unbuffered for immediate output */
    setvbuf(stdout, NULL, _IONBF, 0);
    return 0;
}

void io_shutdown(void) {
    /* Flush any remaining output */
    fflush(stdout);
}

void io_putchar(int c) {
    putchar(c);
}

void io_print(const char *str) {
    fputs(str, stdout);
}

void io_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

int io_getchar(void) {
    return getchar();
}

int io_getline(char *buffer, int max_len) {
    if (!fgets(buffer, max_len, stdin)) {
        return -1;
    }

    /* Remove trailing newline if present */
    int len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') {
        buffer[len-1] = '\0';
        len--;
    }

    return len;
}

bool io_input_ready(void) {
    /* For stdio, we can't easily check without blocking */
    /* This is mainly for future use with select/poll */
    return false;
}

void io_flush(void) {
    fflush(stdout);
}

FILE *io_fopen_read(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        io_warn("Failed to open file '%s' for reading: %s",
                filename, strerror(errno));
    }
    return fp;
}

FILE *io_fopen_write(const char *filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        io_warn("Failed to open file '%s' for writing: %s",
                filename, strerror(errno));
    }
    return fp;
}

void io_fclose(FILE *fp) {
    if (fp) {
        fclose(fp);
    }
}

void io_fatal(const char *fmt, ...) {
    va_list args;

    fprintf(stderr, "\nFATAL ERROR: ");

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
}

void io_warn(const char *fmt, ...) {
    va_list args;

    fprintf(stderr, "Warning: ");

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}
