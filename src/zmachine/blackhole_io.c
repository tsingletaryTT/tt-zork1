/*
 * blackhole_io.c - Blackhole-specific I/O implementation for Frotz Z-machine
 *
 * This implements the I/O abstraction layer for running Frotz on Tenstorrent
 * Blackhole RISC-V cores, using DRAM buffers instead of file I/O.
 */

#include "blackhole_io.h"
#include <string.h>
#include <stdio.h>  /* For SEEK_* constants */

/* Global I/O state */
blackhole_io_state_t g_blackhole_io = {0};

/**
 * Initialize Blackhole I/O system with DRAM buffer pointers
 */
void blackhole_io_init(
    uint32_t game_data_addr,
    uint32_t game_data_size,
    uint32_t input_addr,
    uint32_t input_size,
    uint32_t output_addr,
    uint32_t output_size
) {
    /* Initialize game data buffer (read-only) */
    g_blackhole_io.game_data = (const uint8_t *)game_data_addr;
    g_blackhole_io.game_data_size = game_data_size;
    g_blackhole_io.game_data_pos = 0;

    /* Initialize input buffer (read from host) */
    g_blackhole_io.input_buffer = (const char *)input_addr;
    g_blackhole_io.input_buffer_size = input_size;
    g_blackhole_io.input_read_pos = 0;

    /* Initialize output buffer (write to host) */
    g_blackhole_io.output_buffer = (char *)output_addr;
    g_blackhole_io.output_buffer_size = output_size;
    g_blackhole_io.output_write_pos = 0;

    /* Clear output buffer */
    if (g_blackhole_io.output_buffer && g_blackhole_io.output_buffer_size > 0) {
        memset(g_blackhole_io.output_buffer, 0, g_blackhole_io.output_buffer_size);
    }

#ifdef BUILD_BLACKHOLE_DEBUG
    /* Debug output using DPRINT (if enabled) */
    DPRINT << "Blackhole I/O initialized:" << ENDL();
    DPRINT << "  Game data: " << game_data_size << " bytes at 0x"
           << HEX(game_data_addr) << ENDL();
    DPRINT << "  Input buffer: " << input_size << " bytes at 0x"
           << HEX(input_addr) << ENDL();
    DPRINT << "  Output buffer: " << output_size << " bytes at 0x"
           << HEX(output_addr) << ENDL();
#endif
}

/**
 * Read game data from DRAM buffer (replacement for fread)
 */
size_t blackhole_read_game_data(void *dest, size_t size, size_t count) {
    if (!dest || !g_blackhole_io.game_data) {
        return 0;
    }

    size_t bytes_to_read = size * count;
    size_t bytes_available = g_blackhole_io.game_data_size - g_blackhole_io.game_data_pos;

    if (bytes_to_read > bytes_available) {
        bytes_to_read = bytes_available;
    }

    if (bytes_to_read == 0) {
        return 0;
    }

    /* Copy from game data buffer to destination */
    memcpy(dest,
           g_blackhole_io.game_data + g_blackhole_io.game_data_pos,
           bytes_to_read);

    g_blackhole_io.game_data_pos += bytes_to_read;

    return bytes_to_read / size;  /* Return number of complete elements read */
}

/**
 * Seek within game data buffer (replacement for fseek)
 */
int blackhole_seek_game_data(long offset, int whence) {
    uint32_t new_pos;

    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;

        case SEEK_CUR:
            new_pos = g_blackhole_io.game_data_pos + offset;
            break;

        case SEEK_END:
            new_pos = g_blackhole_io.game_data_size + offset;
            break;

        default:
            return -1;  /* Invalid whence */
    }

    /* Check bounds */
    if (new_pos > g_blackhole_io.game_data_size) {
        return -1;  /* Seek past end of buffer */
    }

    g_blackhole_io.game_data_pos = new_pos;
    return 0;
}

/**
 * Get current position in game data buffer (replacement for ftell)
 */
long blackhole_tell_game_data(void) {
    return (long)g_blackhole_io.game_data_pos;
}

/**
 * Read a line of input from DRAM input buffer (replacement for fgets)
 *
 * Input format: Null-terminated string in input buffer.
 * Host writes command, kernel reads it, host waits for output before next command.
 */
char *blackhole_read_line(char *dest, size_t max_len) {
    if (!dest || max_len == 0 || !g_blackhole_io.input_buffer) {
        return NULL;
    }

    /* Input buffer contains a null-terminated string from host */
    const char *input_str = g_blackhole_io.input_buffer;

    /* Calculate string length with bounds check (strnlen not available in all environments) */
    size_t input_len = 0;
    while (input_len < g_blackhole_io.input_buffer_size && input_str[input_len] != '\0') {
        input_len++;
    }

    if (input_len == 0) {
        /* No input available */
        return NULL;
    }

    /* Copy input to destination, respecting max_len */
    size_t copy_len = input_len;
    if (copy_len >= max_len) {
        copy_len = max_len - 1;  /* Leave room for null terminator */
    }

    strncpy(dest, input_str, copy_len);
    dest[copy_len] = '\0';

    /* Add newline if not present (Frotz expects it) */
    if (copy_len > 0 && dest[copy_len - 1] != '\n') {
        if (copy_len + 1 < max_len) {
            dest[copy_len] = '\n';
            dest[copy_len + 1] = '\0';
        }
    }

    return dest;
}

/**
 * Write a string to DRAM output buffer (replacement for printf)
 */
int blackhole_write_string(const char *str) {
    if (!str || !g_blackhole_io.output_buffer) {
        return -1;
    }

    size_t str_len = strlen(str);
    size_t space_available = g_blackhole_io.output_buffer_size -
                             g_blackhole_io.output_write_pos;

    if (space_available == 0) {
        /* Buffer full */
        return -1;
    }

    /* Write as much as fits */
    size_t write_len = str_len;
    if (write_len > space_available - 1) {  /* Leave room for null terminator */
        write_len = space_available - 1;
    }

    memcpy(g_blackhole_io.output_buffer + g_blackhole_io.output_write_pos,
           str,
           write_len);

    g_blackhole_io.output_write_pos += write_len;

    /* Always maintain null termination */
    g_blackhole_io.output_buffer[g_blackhole_io.output_write_pos] = '\0';

    return (int)write_len;
}

/**
 * Flush output buffer (no-op for DRAM buffers)
 */
void blackhole_flush_output(void) {
    /* In DRAM buffer model, writes are immediately visible to host.
     * Nothing to flush. */
}
