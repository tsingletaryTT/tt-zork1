/**
 * frotz_os.c - Frotz OS Interface Implementation
 *
 * This file implements all the os_* functions required by Frotz.
 * It provides a minimal text-only interface suitable for Zork I (Z-machine v3).
 *
 * Based on Frotz dumb interface, but adapted for our I/O abstraction layer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include "../io/io.h"
#include "frotz/src/common/frotz.h"

/* Global Frotz setup structure (defined in Frotz common) */
f_setup_t f_setup;

/* Screen dimensions */
static int screen_width = 80;
static int screen_height = 24;

/* Forward declarations for initialization */
extern z_header_t z_header;

/* Story file handle */
static FILE *story_fp = NULL;

/* Command line arguments */
static char *story_name = NULL;

/*
 * os_init_setup - Initialize f_setup structure
 */
void os_init_setup(void) {
    /* Initialize the setup structure with default values */
    f_setup.attribute_assignment = 0;
    f_setup.attribute_testing = 0;
    f_setup.context_lines = 0;
    f_setup.object_locating = 0;
    f_setup.object_movement = 0;
    f_setup.left_margin = 0;
    f_setup.right_margin = 0;
    f_setup.ignore_errors = 0;
    f_setup.interpreter_number = INTERP_DEC_20;  /* DEC System 20 */
    f_setup.err_report_mode = ERR_REPORT_ONCE;
    f_setup.restore_mode = 0;
    f_setup.piracy = 0;
    f_setup.undo_slots = MAX_UNDO_SLOTS;
    f_setup.expand_abbreviations = 0;
    f_setup.script_cols = 80;
    f_setup.sound = 1;
    f_setup.format = FORMAT_NORMAL;
}

/*
 * os_process_arguments - Handle command line arguments
 */
void os_process_arguments(int argc, char *argv[]) {
    if (argc < 2) {
        io_fatal("Usage: %s <story-file.z3>", argv[0]);
    }

    story_name = argv[1];

    /* Set up f_setup.story_file for Frotz to use */
    f_setup.story_file = story_name;
    f_setup.story_name = story_name;

    /* Simple argument processing - just get the story file for now */
    /* TODO: Add more options (-w width, -h height, etc.) */
}

/*
 * os_init_screen - Initialize the screen/display
 */
void os_init_screen(void) {
    /* Configure Z-machine header for our interpreter */
    if (z_header.version == V3 && f_setup.tandy) {
        z_header.config |= CONFIG_TANDY;
    }

    if (z_header.version >= V5 && f_setup.undo_slots == 0) {
        z_header.flags &= ~UNDO_FLAG;
    }

    /* Set screen dimensions */
    z_header.screen_rows = screen_height;
    z_header.screen_cols = screen_width;

    /* Set interpreter number and version */
    if (f_setup.interpreter_number == INTERP_DEFAULT) {
        z_header.interpreter_number = (z_header.version == 6) ? INTERP_MSDOS : INTERP_DEC_20;
    } else {
        z_header.interpreter_number = f_setup.interpreter_number;
    }

    z_header.interpreter_version = 'F';  /* Frotz */
}

/*
 * os_reset_screen - Reset screen to initial state
 */
void os_reset_screen(void) {
    io_flush();
}

/*
 * os_load_story - Open and return the story file
 */
FILE *os_load_story(void) {
    if (!story_name) {
        io_fatal("No story file specified");
    }

    story_fp = io_fopen_read(story_name);
    if (!story_fp) {
        io_fatal("Cannot open story file: %s", story_name);
    }

    return story_fp;
}

/*
 * os_display_char - Display a single character
 *
 * With USE_UTF8 defined, zchar is a 16-bit Unicode value.
 * We need to encode it as UTF-8 for output and filter control codes.
 */
void os_display_char(zchar c) {
    /* DEBUG: Log first 50 characters */
    static int char_count = 0;
    if (char_count++ < 50) {
        fprintf(stderr, "[%02x]", c);
    }

    /* Handle special Z-machine control codes */
    if (c == ZC_RETURN) {
        /* Convert CR to LF for newline */
        io_putchar('\n');
        return;
    } else if (c == ZC_GAP) {
        io_putchar(' ');
        io_putchar(' ');
        return;
    } else if (c == ZC_INDENT) {
        io_putchar(' ');
        io_putchar(' ');
        io_putchar(' ');
        return;
    } else if (c < ZC_ASCII_MIN) {
        /* Skip other control codes (style changes, etc.) */
        return;
    }

#ifdef USE_UTF8
    /* UTF-8 encoding for Unicode characters */
    if (c > 0x7ff) {
        /* 3-byte UTF-8 sequence for U+0800..U+FFFF */
        io_putchar(0xe0 | ((c >> 12) & 0xf));
        io_putchar(0x80 | ((c >> 6) & 0x3f));
        io_putchar(0x80 | (c & 0x3f));
    } else if (c > 0x7f) {
        /* 2-byte UTF-8 sequence for U+0080..U+07FF */
        io_putchar(0xc0 | ((c >> 6) & 0x1f));
        io_putchar(0x80 | (c & 0x3f));
    } else {
        /* ASCII character (0..127) */
        io_putchar(c);
    }
#else
    /* Non-UTF8 mode: direct output */
    io_putchar(c);
#endif
}

/*
 * os_display_string - Display a null-terminated string
 *
 * NOTE: Must call os_display_char for each character
 * to ensure proper filtering and UTF-8 encoding!
 */
void os_display_string(const zchar *s) {
    while (*s) {
        os_display_char(*s++);
    }
}

/*
 * os_read_line - Read a line of input
 *
 * max: maximum number of characters to read
 * buf: buffer to store input (zchar format)
 * timeout: timeout in tenths of a second (0 = no timeout)
 * timeout_routine: routine to call on timeout (0 = none)
 * read_cont: continuation mode (not used in simple implementation)
 */
zchar os_read_line(int max, zchar *buf, int timeout, int timeout_routine, int read_cont) {
    (void)timeout; (void)timeout_routine; (void)read_cont;  /* Unused for now */
    char input_buffer[INPUT_BUFFER_SIZE];

    /* Show prompt */
    io_printf("\n>");
    io_flush();

    /* Read input (ignore timeout for now - TODO: implement with select/poll) */
    int len = io_getline(input_buffer, INPUT_BUFFER_SIZE);
    if (len < 0) {
        /* EOF or error */
        os_quit(EXIT_SUCCESS);
    }

    /* Convert to zchar and copy to buffer */
    int i;
    for (i = 0; i < len && i < max; i++) {
        buf[i] = (zchar)input_buffer[i];
    }

    /* Return terminating character (newline) */
    return ZC_RETURN;
}

/*
 * os_read_key - Read a single keystroke
 */
zchar os_read_key(int timeout, int timeout_routine) {
    (void)timeout; (void)timeout_routine;  /* Unused for now */
    int c = io_getchar();
    if (c == EOF) {
        os_quit(EXIT_SUCCESS);
    }
    return (zchar)c;
}

/*
 * os_read_file_name - Prompt for a file name
 */
char *os_read_file_name(const char *default_name, int flag) {
    static char file_name[FILENAME_MAX];
    char prompt[256];

    sprintf(prompt, "%s [%s]: ",
            (flag == FILE_SAVE) ? "Save game" :
            (flag == FILE_RESTORE) ? "Restore game" :
            (flag == FILE_SCRIPT) ? "Script file" :
            (flag == FILE_RECORD) ? "Command file" :
            "File name",
            default_name);

    io_print(prompt);
    io_flush();

    if (io_getline(file_name, FILENAME_MAX) < 0) {
        return NULL;
    }

    /* If empty, use default */
    if (file_name[0] == '\0') {
        return (char *)default_name;
    }

    return file_name;
}

/*
 * os_quit - Exit the interpreter
 */
void os_quit(int status) {
    io_shutdown();
    exit(status);
}

/*
 * os_restart_game - Restart the game
 */
void os_restart_game(int stage) {
    /* stage indicates what phase of restart:
     * 0 = before state reset, 1 = after state reset */
    if (stage == 1) {
        /* Rewind story file */
        if (story_fp) {
            rewind(story_fp);
        }
    }
}

/*
 * os_random_seed - Return random seed
 */
int os_random_seed(void) {
    return (int)time(NULL);
}

/*
 * os_fatal - Fatal error message and exit
 */
void os_fatal(const char *fmt, ...) {
    va_list args;
    char buffer[1024];

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    io_fatal("%s", buffer);
}

/*
 * os_warn - Warning message
 */
void os_warn(const char *fmt, ...) {
    va_list args;
    char buffer[1024];

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    io_warn("%s", buffer);
}

/*
 * os_beep - Beep the terminal
 */
void os_beep(int volume) {
    /* Simple beep - print bell character */
    io_putchar('\a');
}

/*
 * os_more_prompt - Display "more" prompt and wait for key
 *
 * For a simple text interface, we don't implement paging,
 * so we just skip the MORE prompt.
 */
void os_more_prompt(void) {
    /* Skip MORE prompts for now - no paging in simple interface */
}

/*
 * Stubs for graphics/sound/advanced features (not needed for Zork I)
 */

void os_draw_picture(int picture, int y, int x) {
    /* No graphics support */
}

int os_picture_data(int picture, int *height, int *width) {
    return FALSE;
}

void os_init_sound(void) {
    /* No sound support */
}

void os_start_sample(int number, int volume, int repeats, zword eos) {
    /* No sound support */
}

void os_stop_sample(int number) {
    /* No sound support */
}

void os_prepare_sample(int number) {
    /* No sound support */
}

void os_finish_with_sample(int number) {
    /* No sound support */
}

/*
 * Screen manipulation functions (simplified for text-only)
 */

void os_erase_area(int top, int left, int bottom, int right, int win) {
    /* Text-only - just print newline */
    io_putchar('\n');
}

void os_scroll_area(int top, int left, int bottom, int right, int units) {
    /* Not implemented for simple text interface */
}

void os_set_cursor(int row, int col) {
    /* Text-only - no cursor positioning */
}

void os_set_colour(int fg, int bg) {
    /* Text-only - no color support */
}

void os_set_font(int font) {
    /* Text-only - no font support */
}

void os_set_text_style(int style) {
    /* Text-only - no text style support */
}

int os_get_text_style(void) {
    return 0;
}

int os_char_width(zchar c) {
    return 1;  /* Fixed width for text mode */
}

int os_string_width(const zchar *s) {
    int width = 0;
    while (*s++) {
        width++;
    }
    return width;
}

int os_font_data(int font, int *height, int *width) {
    *height = 1;
    *width = 1;
    return TRUE;
}

int os_peek_colour(void) {
    return 0;
}

int os_check_unicode(int font, zchar c) {
    /* Basic ASCII only for now */
    return (c < 128) ? TRUE : FALSE;
}

zword os_to_true_colour(int colour) {
    return 0;
}

int os_from_true_colour(zword colour) {
    return 0;
}

/*
 * File I/O helpers
 */

int os_storyfile_seek(FILE *fp, long offset, int whence) {
    return fseek(fp, offset, whence);
}

int os_storyfile_tell(FILE *fp) {
    return ftell(fp);
}

/*
 * os_tick - Called regularly by the interpreter
 */
void os_tick(void) {
    /* Nothing to do in simple implementation */
}

/*
 * os_repaint_window - Repaint a window (for screen resize)
 */
bool os_repaint_window(int win, int ypos_old, int ypos_new, int xpos,
                       int ysize, int xsize) {
    return FALSE;  /* No window support */
}
