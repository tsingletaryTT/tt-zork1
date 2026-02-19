/**
 * tui_output.c - TUI-aware Output Helper Implementation
 */

#include "tui_output.h"
#include <stdio.h>
#include <stdarg.h>

/* Forward declarations for TUI functions */
extern int tui_is_enabled(void);
extern void tui_display_text(const char *text);

void tui_output(const char *format, ...) {
    char buffer[1024];
    va_list args;

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (tui_is_enabled()) {
        /* Route through TUI system when active */
        tui_display_text(buffer);
    } else {
        /* Use stderr when TUI disabled */
        fprintf(stderr, "%s", buffer);
    }
}
