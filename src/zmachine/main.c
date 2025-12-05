/**
 * main.c - Main entry point for Zork on Tenstorrent
 *
 * This file provides a thin wrapper that calls into Frotz.
 * Most of the real work is done in Frotz's common/main.c
 */

#include <stdlib.h>
#include "../io/io.h"
#include "frotz/src/common/frotz.h"

/* Frotz functions (defined in frotz/src/common/ ) */
extern void interpret(void);
extern void init_memory(void);
extern void init_undo(void);
extern void init_process(void);
extern void init_buffer(void);
extern void init_header(void);
extern void init_setup(void);
extern void init_sound(void);
extern void init_err(void);
extern void z_restart(void);
extern void reset_screen(void);
extern void reset_memory(void);
extern void branch(bool);

/* Global variables required by Frotz (normally defined in Frotz's main.c) */
char *story_name = NULL;
enum story story_id = UNKNOWN;
long story_size = 0;

/* Setup and header data */
f_setup_t f_setup;
z_header_t z_header;

/* Stack data */
zword stack[STACK_SIZE];
zword *sp = NULL;
zword *fp = NULL;
zword frame_count = 0;

/* IO streams */
bool ostream_screen = TRUE;
bool ostream_script = FALSE;
bool ostream_memory = FALSE;
bool ostream_record = FALSE;
bool istream_replay = FALSE;
bool message = FALSE;

/* Current window and mouse data */
int cwin = 0;
int mwin = 0;
int mouse_y = 0;
int mouse_x = 0;

/* Window attributes */
bool enable_wrapping = FALSE;
bool enable_scripting = FALSE;
bool enable_scrolling = FALSE;
bool enable_buffering = FALSE;

/* Options */
int option_sound = 1;
char *option_zcode_path = NULL;

/* Size of memory to reserve (in bytes) */
long reserve_mem = 0;

/* Additional globals */
bool need_newline_at_exit = FALSE;

/*
 * z_piracy - branch if the story file is a legal copy
 */
void z_piracy(void) {
    branch(!f_setup.piracy);
}

/**
 * main - Entry point
 *
 * NOTE: Initialization order must match Frotz's main.c exactly
 * for proper operation. Do not reorder these calls!
 */
int main(int argc, char *argv[]) {
    /* Initialize I/O system first (our addition, not in Frotz) */
    if (io_init() != 0) {
        io_fatal("Failed to initialize I/O system");
    }

    /* Print welcome message */
    io_printf("Zork on Tenstorrent - Z-Machine Interpreter\n");
    io_printf("Based on Frotz " VERSION "\n\n");

    /* CRITICAL: Follow Frotz's exact initialization sequence */
    init_header();
    init_setup();
    os_init_setup();
    os_process_arguments(argc, argv);
    init_buffer();
    init_err();
    init_memory();
    init_process();
    init_sound();
    os_init_screen();
    init_undo();
    z_restart();

    /* Run the Z-machine interpreter main loop */
    interpret();

    /* Clean up and exit */
    reset_screen();
    reset_memory();
    os_reset_screen();
    os_quit(EXIT_SUCCESS);

    return 0;  /* Never reached */
}
