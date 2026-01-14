/**
 * game_state.c - Game End State Detection Implementation
 *
 * EDUCATIONAL PROJECT - Heavily commented for learning!
 *
 * This implements pattern-based detection of game endings. It demonstrates:
 * - String pattern matching for game state
 * - Stateful detection (accumulating evidence over multiple outputs)
 * - Clean separation between detection and action
 *
 * ## Implementation Strategy
 *
 * **Pattern Matching**:
 * We maintain a simple state machine that watches for known death/victory patterns.
 * Patterns are case-insensitive and match substrings.
 *
 * **Why Not More Complex?**:
 * - Could use regex, but simple string matching is sufficient
 * - Could parse Z-machine flags, but that's game-specific
 * - This approach works for all text-based IF games
 *
 * ## Known Patterns
 *
 * Zork 1 patterns:
 * - Death: "****  You have died  ****", "You are dead", "killed"
 * - Victory: "****  You have won  ****", "Congratulations"
 *
 * These patterns are common across most Infocom games.
 */

#include "game_state.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Game state (singleton) */
static struct {
    game_end_reason_t reason;
    int initialized;
} g_state = {
    .reason = GAME_END_UNKNOWN,
    .initialized = 0
};

/**
 * Helper: Case-insensitive substring search
 */
static int contains_pattern(const char *text, const char *pattern) {
    if (!text || !pattern) {
        return 0;
    }

    /* Convert both to lowercase for comparison */
    char text_lower[1024];
    char pattern_lower[256];

    size_t text_len = strlen(text);
    size_t pattern_len = strlen(pattern);

    if (text_len >= sizeof(text_lower)) {
        text_len = sizeof(text_lower) - 1;
    }
    if (pattern_len >= sizeof(pattern_lower)) {
        pattern_len = sizeof(pattern_lower) - 1;
    }

    for (size_t i = 0; i < text_len; i++) {
        text_lower[i] = tolower((unsigned char)text[i]);
    }
    text_lower[text_len] = '\0';

    for (size_t i = 0; i < pattern_len; i++) {
        pattern_lower[i] = tolower((unsigned char)pattern[i]);
    }
    pattern_lower[pattern_len] = '\0';

    return strstr(text_lower, pattern_lower) != NULL;
}

/*
 * Public API Implementation
 */

int game_state_init(void) {
    g_state.reason = GAME_END_UNKNOWN;
    g_state.initialized = 1;

    fprintf(stderr, "Game state detection: Initialized\n");
    return 0;
}

void game_state_watch_output(const char *text) {
    if (!g_state.initialized || !text) {
        return;
    }

    /* Skip empty or whitespace-only text */
    while (*text && isspace((unsigned char)*text)) {
        text++;
    }
    if (*text == '\0') {
        return;
    }

    /* Check for death patterns */
    if (contains_pattern(text, "you have died") ||
        contains_pattern(text, "you are dead") ||
        contains_pattern(text, "you have been killed") ||
        contains_pattern(text, "****  You have died  ****")) {

        if (g_state.reason != GAME_END_DEATH) {
            fprintf(stderr, "Game state: DEATH detected\n");
            g_state.reason = GAME_END_DEATH;
        }
        return;
    }

    /* Check for victory patterns */
    if (contains_pattern(text, "you have won") ||
        contains_pattern(text, "congratulations") ||
        contains_pattern(text, "****  You have won  ****") ||
        contains_pattern(text, "you have completed")) {

        if (g_state.reason != GAME_END_VICTORY) {
            fprintf(stderr, "Game state: VICTORY detected\n");
            g_state.reason = GAME_END_VICTORY;
        }
        return;
    }
}

void game_state_set_user_quit(void) {
    if (!g_state.initialized) {
        return;
    }

    fprintf(stderr, "Game state: USER QUIT detected\n");
    g_state.reason = GAME_END_USER_QUIT;
}

int game_state_should_show_map(void) {
    if (!g_state.initialized) {
        return 0;
    }

    /* Show map for death or victory, not for user quit or unknown */
    return (g_state.reason == GAME_END_DEATH ||
            g_state.reason == GAME_END_VICTORY);
}

game_end_reason_t game_state_get_reason(void) {
    return g_state.reason;
}

const char *game_state_get_reason_string(void) {
    switch (g_state.reason) {
        case GAME_END_DEATH:
            return "Death";
        case GAME_END_VICTORY:
            return "Victory";
        case GAME_END_USER_QUIT:
            return "User Quit";
        case GAME_END_UNKNOWN:
        default:
            return "Unknown";
    }
}

void game_state_reset(void) {
    if (!g_state.initialized) {
        return;
    }

    g_state.reason = GAME_END_UNKNOWN;
    fprintf(stderr, "Game state: Reset\n");
}

void game_state_shutdown(void) {
    g_state.reason = GAME_END_UNKNOWN;
    g_state.initialized = 0;

    fprintf(stderr, "Game state detection: Shutdown\n");
}
