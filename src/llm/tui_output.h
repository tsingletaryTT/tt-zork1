/**
 * tui_output.h - TUI-aware Output Helper
 *
 * Simple utility to route initialization messages through the TUI
 * when it's active, or stderr when it's disabled.
 */

#ifndef TT_ZORK_LLM_TUI_OUTPUT_H
#define TT_ZORK_LLM_TUI_OUTPUT_H

/**
 * Output a formatted message that respects TUI mode
 *
 * When TUI is active: Routes to game window via tui_display_text()
 * When TUI is disabled: Routes to stderr via fprintf()
 *
 * Usage:
 *   tui_output("Initializing...\n");
 *   tui_output("Loading %s from %s\n", name, path);
 */
void tui_output(const char *format, ...);

#endif /* TT_ZORK_LLM_TUI_OUTPUT_H */
