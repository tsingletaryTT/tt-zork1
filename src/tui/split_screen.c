/**
 * split_screen.c - Split-Screen TUI Implementation
 *
 * This file implements a modern terminal UI for Zork using ncurses.
 * See split_screen.h for architecture and usage details.
 */

#include "split_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

/* ncurses - try ncursesw (wide char) first, fall back to regular ncurses */
#ifdef HAVE_NCURSESW
#include <ncursesw/ncurses.h>
#else
#include <ncurses.h>
#endif

/* Forward declarations for LLM integration */
extern int llm_router_is_task_ready(int task);  /* From llm_router.h */
extern int auto_player_is_enabled(void);        /* From auto_player.h */
extern const char* auto_player_get_persona_name(void); /* From auto_player.h */

/* LLM task types (from llm_router.h) */
#define LLM_TASK_TRANSLATE  0
#define LLM_TASK_VISUALIZE  1
#define LLM_TASK_NARRATE    2

/* Forward declarations for journey/inventory access */
typedef struct {
    int count;
    struct {
        char room_name[32];
        int location_id;
    } *moves;
} journey_history_t;

extern journey_history_t* journey_get_history(void);  /* From journey/tracker.h */

/* Z-machine object access (we'll provide stubs if not available) */
extern unsigned short get_player_object(void);
extern unsigned short get_child(unsigned short obj);
extern unsigned short get_sibling(unsigned short obj);
extern void get_object_name(unsigned short obj, char *buf, int size);

/**
 * Weak symbol stubs for optional dependencies
 * ====================
 * These provide default implementations when LLM/journey systems aren't linked.
 * If the real functions are available, the linker will use those instead.
 */
#ifndef __APPLE__
/* GCC/Linux weak symbols */
int __attribute__((weak)) llm_router_is_task_ready(int task) { return 0; }
int __attribute__((weak)) auto_player_is_enabled(void) { return 0; }
const char* __attribute__((weak)) auto_player_get_persona_name(void) { return "None"; }
journey_history_t* __attribute__((weak)) journey_get_history(void) { return NULL; }
#else
/* macOS doesn't support weak symbols the same way - use pragma weak */
#pragma weak llm_router_is_task_ready
#pragma weak auto_player_is_enabled
#pragma weak auto_player_get_persona_name
#pragma weak journey_get_history
/* Provide fallback implementations */
static int stub_llm_router_is_task_ready(int task) { return 0; }
static int stub_auto_player_is_enabled(void) { return 0; }
static const char* stub_auto_player_get_persona_name(void) { return "None"; }
static journey_history_t* stub_journey_get_history(void) { return NULL; }
#define llm_router_is_task_ready (llm_router_is_task_ready ? llm_router_is_task_ready : stub_llm_router_is_task_ready)
#define auto_player_is_enabled (auto_player_is_enabled ? auto_player_is_enabled : stub_auto_player_is_enabled)
#define auto_player_get_persona_name (auto_player_get_persona_name ? auto_player_get_persona_name : stub_auto_player_get_persona_name)
#define journey_get_history (journey_get_history ? journey_get_history : stub_journey_get_history)
#endif

/**
 * Layout Configuration
 * ====================
 * These constants define the split-screen layout proportions.
 */
#define HEADER_HEIGHT 1
#define FOOTER_HEIGHT 1
#define SIDEBAR_WIDTH_PERCENT 30  /* Sidebar is 30% of terminal width */

#define MIN_TERMINAL_WIDTH 80
#define MIN_TERMINAL_HEIGHT 24

/**
 * Window State
 * ============
 * Global state for all ncurses windows.
 */
static bool g_tui_enabled = false;
static bool g_tui_initialized = false;

static WINDOW *g_header_win = NULL;
static WINDOW *g_game_win = NULL;
static WINDOW *g_sidebar_win = NULL;
static WINDOW *g_footer_win = NULL;

/* Sidebar subwindows */
static WINDOW *g_art_win = NULL;
static WINDOW *g_inventory_win = NULL;
static WINDOW *g_journey_win = NULL;

/* Layout dimensions (calculated on init/resize) */
static int g_terminal_height = 0;
static int g_terminal_width = 0;
static int g_game_width = 0;
static int g_sidebar_width = 0;
static int g_main_height = 0;

/**
 * Color Pairs
 * ===========
 * ncurses color pair definitions for the TUI.
 */
enum {
    COLOR_PAIR_HEADER = 1,
    COLOR_PAIR_LLM_ACTIVE = 2,
    COLOR_PAIR_LLM_INACTIVE = 3,
    COLOR_PAIR_SIDEBAR_BORDER = 4,
    COLOR_PAIR_GAME_TEXT = 5
};

/**
 * Initialize color pairs if terminal supports color
 */
static void init_colors(void)
{
    if (!has_colors()) {
        return;
    }

    start_color();
    use_default_colors();  /* Allow terminal default colors */

    /* Header: Cyan on default background */
    init_pair(COLOR_PAIR_HEADER, COLOR_CYAN, -1);

    /* LLM active: Green on default background */
    init_pair(COLOR_PAIR_LLM_ACTIVE, COLOR_GREEN, -1);

    /* LLM inactive: Red on default background */
    init_pair(COLOR_PAIR_LLM_INACTIVE, COLOR_RED, -1);

    /* Sidebar border: Yellow on default background */
    init_pair(COLOR_PAIR_SIDEBAR_BORDER, COLOR_YELLOW, -1);

    /* Game text: Default colors */
    init_pair(COLOR_PAIR_GAME_TEXT, -1, -1);
}

/**
 * Calculate layout dimensions based on terminal size
 */
static void calculate_layout(void)
{
    getmaxyx(stdscr, g_terminal_height, g_terminal_width);

    /* Main area height (minus header and footer) */
    g_main_height = g_terminal_height - HEADER_HEIGHT - FOOTER_HEIGHT;

    /* Calculate sidebar width (30% of terminal) */
    g_sidebar_width = (g_terminal_width * SIDEBAR_WIDTH_PERCENT) / 100;
    g_game_width = g_terminal_width - g_sidebar_width;

    /* Ensure minimum widths */
    if (g_sidebar_width < 20) g_sidebar_width = 20;
    if (g_game_width < 40) g_game_width = g_terminal_width - g_sidebar_width;
}

/**
 * Create all ncurses windows for the layout
 */
static int create_windows(void)
{
    /* Clear any existing windows */
    if (g_header_win) delwin(g_header_win);
    if (g_game_win) delwin(g_game_win);
    if (g_sidebar_win) delwin(g_sidebar_win);
    if (g_footer_win) delwin(g_footer_win);
    if (g_art_win) delwin(g_art_win);
    if (g_inventory_win) delwin(g_inventory_win);
    if (g_journey_win) delwin(g_journey_win);

    /* Calculate layout dimensions */
    calculate_layout();

    /* Create main regions */
    g_header_win = newwin(HEADER_HEIGHT, g_terminal_width, 0, 0);
    g_game_win = newwin(g_main_height, g_game_width, HEADER_HEIGHT, 0);
    g_sidebar_win = newwin(g_main_height, g_sidebar_width, HEADER_HEIGHT, g_game_width);
    g_footer_win = newwin(FOOTER_HEIGHT, g_terminal_width, g_terminal_height - 1, 0);

    if (!g_header_win || !g_game_win || !g_sidebar_win || !g_footer_win) {
        return -1;
    }

    /* Enable scrolling for game window */
    scrollok(g_game_win, TRUE);
    idlok(g_game_win, TRUE);

    /* Create sidebar subwindows (divide sidebar into 3 sections) */
    int art_height = g_main_height / 3;
    int inventory_height = g_main_height / 3;
    int journey_height = g_main_height - art_height - inventory_height;

    g_art_win = derwin(g_sidebar_win, art_height, g_sidebar_width, 0, 0);
    g_inventory_win = derwin(g_sidebar_win, inventory_height, g_sidebar_width, art_height, 0);
    g_journey_win = derwin(g_sidebar_win, journey_height, g_sidebar_width, art_height + inventory_height, 0);

    if (!g_art_win || !g_inventory_win || !g_journey_win) {
        return -1;
    }

    return 0;
}

/**
 * Initialize the TUI system
 */
int tui_init(void)
{
    /* Check if TUI should be disabled via environment variable */
    const char *disable_env = getenv("ZORK_TUI_DISABLE");
    if (disable_env && strcmp(disable_env, "1") == 0) {
        g_tui_enabled = false;
        g_tui_initialized = true;
        return 0;  /* Not an error, just disabled */
    }

    /* Set locale for UTF-8 support (emojis, etc.) */
    setlocale(LC_ALL, "");

    /* Initialize ncurses */
    if (!initscr()) {
        fprintf(stderr, "Error: Failed to initialize ncurses\n");
        return -1;
    }

    /* Check terminal size */
    int h, w;
    getmaxyx(stdscr, h, w);
    if (w < MIN_TERMINAL_WIDTH || h < MIN_TERMINAL_HEIGHT) {
        endwin();
        fprintf(stderr, "Error: Terminal too small (%dx%d). Need at least %dx%d.\n",
                w, h, MIN_TERMINAL_WIDTH, MIN_TERMINAL_HEIGHT);
        return -1;
    }

    /* Configure ncurses */
    cbreak();              /* Disable line buffering */
    noecho();              /* Don't echo input */
    keypad(stdscr, TRUE);  /* Enable arrow keys, etc. */
    curs_set(0);           /* Hide cursor initially */

    /* Initialize colors if available */
    init_colors();

    /* Create window layout */
    if (create_windows() < 0) {
        endwin();
        fprintf(stderr, "Error: Failed to create TUI windows\n");
        return -1;
    }

    /* Initial display */
    tui_update_header();
    tui_update_sidebar();

    /* Display welcome message in game window */
    wattron(g_game_win, A_BOLD);
    wprintw(g_game_win, "Welcome to Zork!\n");
    wprintw(g_game_win, "Enhanced TUI Mode Enabled\n\n");
    wattroff(g_game_win, A_BOLD);
    wrefresh(g_game_win);

    refresh();  /* Refresh stdscr */

    g_tui_enabled = true;
    g_tui_initialized = true;

    return 0;
}

/**
 * Check if TUI is enabled
 */
bool tui_is_enabled(void)
{
    return g_tui_enabled;
}

/**
 * Display text in the game window
 */
void tui_display_text(const char *text)
{
    if (!g_tui_enabled || !g_game_win) {
        /* Fallback to stdout if TUI disabled */
        fprintf(stdout, "%s", text);
        fflush(stdout);
        return;
    }

    /* Add text to game window (will scroll automatically) */
    waddstr(g_game_win, text);
    wrefresh(g_game_win);
}

/**
 * Update the header bar with game mode and LLM status
 */
void tui_update_header(void)
{
    if (!g_tui_enabled || !g_header_win) {
        return;
    }

    werase(g_header_win);

    /* Use color if available */
    if (has_colors()) {
        wattron(g_header_win, COLOR_PAIR(COLOR_PAIR_HEADER) | A_BOLD);
    } else {
        wattron(g_header_win, A_BOLD);
    }

    /* Display title */
    mvwprintw(g_header_win, 0, 0, "ZORK");

    wattroff(g_header_win, A_BOLD);

    /* Display mode (future: query game mode) */
    mvwprintw(g_header_win, 0, 7, "| Mode: ENHANCED");

    /* Display LLM agent status indicators */
    int col = 26;
    mvwprintw(g_header_win, 0, col, "|");
    col += 2;

    /* Translator status */
    int translator_active = llm_router_is_task_ready(LLM_TASK_TRANSLATE);
    if (has_colors()) {
        wattron(g_header_win, COLOR_PAIR(translator_active ? COLOR_PAIR_LLM_ACTIVE : COLOR_PAIR_LLM_INACTIVE));
    }
    mvwprintw(g_header_win, 0, col, "T");
    if (has_colors()) {
        wattroff(g_header_win, COLOR_PAIR(translator_active ? COLOR_PAIR_LLM_ACTIVE : COLOR_PAIR_LLM_INACTIVE));
    }
    mvwprintw(g_header_win, 0, col + 1, "%s", translator_active ? "✓" : "✗");
    col += 4;

    /* Artist status */
    int artist_active = llm_router_is_task_ready(LLM_TASK_VISUALIZE);
    if (has_colors()) {
        wattron(g_header_win, COLOR_PAIR(artist_active ? COLOR_PAIR_LLM_ACTIVE : COLOR_PAIR_LLM_INACTIVE));
    }
    mvwprintw(g_header_win, 0, col, "A");
    if (has_colors()) {
        wattroff(g_header_win, COLOR_PAIR(artist_active ? COLOR_PAIR_LLM_ACTIVE : COLOR_PAIR_LLM_INACTIVE));
    }
    mvwprintw(g_header_win, 0, col + 1, "%s", artist_active ? "✓" : "✗");
    col += 4;

    /* DM status */
    int dm_active = llm_router_is_task_ready(LLM_TASK_NARRATE);
    if (has_colors()) {
        wattron(g_header_win, COLOR_PAIR(dm_active ? COLOR_PAIR_LLM_ACTIVE : COLOR_PAIR_LLM_INACTIVE));
    }
    mvwprintw(g_header_win, 0, col, "DM");
    if (has_colors()) {
        wattroff(g_header_win, COLOR_PAIR(dm_active ? COLOR_PAIR_LLM_ACTIVE : COLOR_PAIR_LLM_INACTIVE));
    }
    mvwprintw(g_header_win, 0, col + 2, "%s", dm_active ? "✓" : "✗");
    col += 5;

    /* Player agent status */
    int player_active = auto_player_is_enabled();
    if (has_colors()) {
        wattron(g_header_win, COLOR_PAIR(player_active ? COLOR_PAIR_LLM_ACTIVE : COLOR_PAIR_LLM_INACTIVE));
    }
    mvwprintw(g_header_win, 0, col, "P");
    if (has_colors()) {
        wattroff(g_header_win, COLOR_PAIR(player_active ? COLOR_PAIR_LLM_ACTIVE : COLOR_PAIR_LLM_INACTIVE));
    }
    mvwprintw(g_header_win, 0, col + 1, "%s", player_active ? "✓" : "✗");

    if (has_colors()) {
        wattroff(g_header_win, COLOR_PAIR(COLOR_PAIR_HEADER));
    }

    wrefresh(g_header_win);
}

/**
 * Update ASCII art display in sidebar
 */
static void update_ascii_art(void)
{
    if (!g_art_win) return;

    werase(g_art_win);

    /* Draw border */
    box(g_art_win, 0, 0);
    if (has_colors()) {
        wattron(g_art_win, COLOR_PAIR(COLOR_PAIR_SIDEBAR_BORDER));
    }
    mvwprintw(g_art_win, 0, 2, " ASCII Art ");
    if (has_colors()) {
        wattroff(g_art_win, COLOR_PAIR(COLOR_PAIR_SIDEBAR_BORDER));
    }

    /* TODO: Read latest ASCII art from scene_visualizer
     * For now, display placeholder */
    int max_lines = getmaxy(g_art_win) - 2;  /* Minus borders */
    int max_cols = getmaxx(g_art_win) - 2;

    /* Placeholder art */
    if (max_lines >= 4) {
        mvwprintw(g_art_win, 2, 2, "🏠");
        mvwprintw(g_art_win, 3, 1, "📬  🔒");
        mvwprintw(g_art_win, 4, 1, "🌿🌿");
    }

    wrefresh(g_art_win);
}

/**
 * Update inventory display in sidebar
 */
static void update_inventory(void)
{
    if (!g_inventory_win) return;

    werase(g_inventory_win);

    /* Draw border */
    box(g_inventory_win, 0, 0);
    if (has_colors()) {
        wattron(g_inventory_win, COLOR_PAIR(COLOR_PAIR_SIDEBAR_BORDER));
    }
    mvwprintw(g_inventory_win, 0, 2, " Inventory ");
    if (has_colors()) {
        wattroff(g_inventory_win, COLOR_PAIR(COLOR_PAIR_SIDEBAR_BORDER));
    }

    /* Get inventory from Z-machine object tree
     * This requires access to Z-machine internals - for now use placeholder */
    int row = 1;
    int max_lines = getmaxy(g_inventory_win) - 2;

    /* TODO: Query actual inventory from Z-machine
     * For now, display placeholder */
    mvwprintw(g_inventory_win, row++, 1, "(empty)");

    wrefresh(g_inventory_win);
}

/**
 * Update journey map display in sidebar
 */
static void update_journey_map(void)
{
    if (!g_journey_win) return;

    werase(g_journey_win);

    /* Draw border */
    box(g_journey_win, 0, 0);
    if (has_colors()) {
        wattron(g_journey_win, COLOR_PAIR(COLOR_PAIR_SIDEBAR_BORDER));
    }
    mvwprintw(g_journey_win, 0, 2, " Journey ");
    if (has_colors()) {
        wattroff(g_journey_win, COLOR_PAIR(COLOR_PAIR_SIDEBAR_BORDER));
    }

    /* Get journey history from tracker */
    journey_history_t *history = journey_get_history();

    int row = 1;
    int max_lines = getmaxy(g_journey_win) - 2;

    if (!history || history->count == 0) {
        mvwprintw(g_journey_win, row, 1, "(no journey yet)");
    } else {
        /* Display last N rooms (whatever fits) */
        int start_idx = (history->count > max_lines) ? (history->count - max_lines) : 0;

        for (int i = start_idx; i < history->count && row < max_lines + 1; i++) {
            /* Show arrow for all but last room, dot for current location */
            const char *connector = (i < history->count - 1) ? " →" : " •";
            mvwprintw(g_journey_win, row++, 1, "%s%s", history->moves[i].room_name, connector);
        }
    }

    wrefresh(g_journey_win);
}

/**
 * Update all sidebar sections
 */
void tui_update_sidebar(void)
{
    if (!g_tui_enabled) return;

    update_ascii_art();
    update_inventory();
    update_journey_map();
}

/**
 * Read user input from footer with line editing
 */
int tui_read_input(const char *prompt, char *buf, int bufsize)
{
    if (!g_tui_enabled || !g_footer_win) {
        /* Fallback to stdin if TUI disabled */
        fprintf(stdout, "%s", prompt);
        fflush(stdout);
        if (fgets(buf, bufsize, stdin)) {
            /* Remove trailing newline */
            int len = strlen(buf);
            if (len > 0 && buf[len-1] == '\n') {
                buf[len-1] = '\0';
                len--;
            }
            return len;
        }
        return -1;
    }

    /* Clear footer and display prompt */
    werase(g_footer_win);
    mvwprintw(g_footer_win, 0, 0, "%s", prompt);
    wrefresh(g_footer_win);

    /* Show cursor */
    curs_set(1);

    /* Enable echo for input */
    echo();

    /* Read line using ncurses (handles editing automatically) */
    int prompt_len = strlen(prompt);
    wmove(g_footer_win, 0, prompt_len);
    wgetnstr(g_footer_win, buf, bufsize - 1);

    /* Disable echo and hide cursor */
    noecho();
    curs_set(0);

    /* Return length of input */
    return strlen(buf);
}

/**
 * Handle terminal resize
 */
void tui_handle_resize(void)
{
    if (!g_tui_enabled) return;

    /* Recreate windows with new dimensions */
    endwin();
    refresh();

    create_windows();

    /* Redraw everything */
    tui_update_header();
    tui_update_sidebar();
    tui_refresh_all();
}

/**
 * Force refresh of all windows
 */
void tui_refresh_all(void)
{
    if (!g_tui_enabled) return;

    if (g_header_win) wrefresh(g_header_win);
    if (g_game_win) wrefresh(g_game_win);
    if (g_sidebar_win) wrefresh(g_sidebar_win);
    if (g_art_win) wrefresh(g_art_win);
    if (g_inventory_win) wrefresh(g_inventory_win);
    if (g_journey_win) wrefresh(g_journey_win);
    if (g_footer_win) wrefresh(g_footer_win);
    refresh();
}

/**
 * Shut down the TUI
 */
void tui_shutdown(void)
{
    if (!g_tui_initialized) return;

    if (g_tui_enabled) {
        /* Clean up ncurses */
        if (g_art_win) delwin(g_art_win);
        if (g_inventory_win) delwin(g_inventory_win);
        if (g_journey_win) delwin(g_journey_win);
        if (g_header_win) delwin(g_header_win);
        if (g_game_win) delwin(g_game_win);
        if (g_sidebar_win) delwin(g_sidebar_win);
        if (g_footer_win) delwin(g_footer_win);

        endwin();
    }

    g_tui_enabled = false;
    g_tui_initialized = false;
}
