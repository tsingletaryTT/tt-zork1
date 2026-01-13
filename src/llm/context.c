/**
 * context.c - Game Context Manager Implementation
 *
 * EDUCATIONAL PROJECT - Heavily commented for learning!
 *
 * This implements a circular buffer for storing game conversation history.
 * See context.h for architecture overview.
 *
 * ## Data Structure: Circular Buffer
 *
 * A circular buffer is like a race track - when you reach the end, you wrap
 * around to the beginning. It's perfect for "rolling history" where we only
 * care about recent events.
 *
 * Visual example (capacity = 4):
 *
 *   Initial:  [empty][empty][empty][empty]
 *              ^write
 *
 *   Add 3:    [Turn1][Turn2][Turn3][empty]
 *                                   ^write
 *
 *   Add 2:    [Turn1][Turn2][Turn3][Turn4]
 *              ^write (wrapped!)
 *
 *   Add 1:    [Turn5][Turn2][Turn3][Turn4]
 *                     ^write
 *   (Turn1 was overwritten by Turn5)
 *
 * ## Memory Layout
 *
 * Each turn has:
 * - output: Game's response (e.g., room description)
 * - user_input: User's natural language
 * - translated: LLM's command translation
 *
 * Total memory per turn: ~2KB
 * For 20 turns: ~40KB total (very reasonable!)
 *
 * ## Thread Safety
 *
 * NOT thread-safe! Game is single-threaded so this is fine.
 * If you add threading later, add mutexes.
 */

#include "context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Maximum size for each component of a turn */
#define MAX_OUTPUT_SIZE 1024        /* Game output */
#define MAX_USER_INPUT_SIZE 512     /* User's NL input */
#define MAX_TRANSLATED_SIZE 256     /* LLM commands */

/**
 * Structure representing one turn of conversation
 *
 * A "turn" is one complete interaction:
 * 1. Game outputs text (room description, results of actions)
 * 2. User provides input (natural language)
 * 3. LLM translates to commands
 */
typedef struct {
    char output[MAX_OUTPUT_SIZE];           /* Accumulated game output */
    char user_input[MAX_USER_INPUT_SIZE];   /* User's natural language */
    char translated[MAX_TRANSLATED_SIZE];   /* LLM's command translation */
    int has_input;                          /* Flag: has user input been recorded? */
} turn_t;

/**
 * Circular buffer state
 */
static struct {
    turn_t *turns;          /* Array of turns */
    size_t capacity;        /* Maximum number of turns */
    size_t write_index;     /* Where to write next */
    size_t count;           /* How many turns stored (up to capacity) */
    turn_t current_turn;    /* Current turn being built */
} context = {0};

/**
 * Helper: Clear a turn structure
 */
static void clear_turn(turn_t *turn) {
    turn->output[0] = '\0';
    turn->user_input[0] = '\0';
    turn->translated[0] = '\0';
    turn->has_input = 0;
}

/*
 * Public API Implementation
 */

int context_init(size_t max_turns) {
    /* Validate input */
    if (max_turns == 0) {
        fprintf(stderr, "Error: max_turns must be > 0\n");
        return -1;
    }

    /* Allocate circular buffer */
    context.turns = calloc(max_turns, sizeof(turn_t));
    if (!context.turns) {
        fprintf(stderr, "Error: Out of memory allocating context buffer\n");
        return -1;
    }

    /* Initialize state */
    context.capacity = max_turns;
    context.write_index = 0;
    context.count = 0;
    clear_turn(&context.current_turn);

    fprintf(stderr, "Context manager: Tracking last %zu turns\n", max_turns);
    return 0;
}

void context_add_output(const char *output) {
    if (!output || !context.turns) {
        return;
    }

    /* Append to current turn's output buffer */
    size_t current_len = strlen(context.current_turn.output);
    size_t output_len = strlen(output);
    size_t available = MAX_OUTPUT_SIZE - current_len - 1;

    if (output_len > available) {
        /* Truncate if necessary */
        strncat(context.current_turn.output, output, available);
        context.current_turn.output[MAX_OUTPUT_SIZE - 1] = '\0';
    } else {
        strcat(context.current_turn.output, output);
    }
}

void context_add_input(const char *user_text, const char *translated_commands) {
    if (!user_text || !context.turns) {
        return;
    }

    /* Record user input and translation in current turn */
    strncpy(context.current_turn.user_input, user_text, MAX_USER_INPUT_SIZE - 1);
    context.current_turn.user_input[MAX_USER_INPUT_SIZE - 1] = '\0';

    if (translated_commands) {
        strncpy(context.current_turn.translated, translated_commands, MAX_TRANSLATED_SIZE - 1);
        context.current_turn.translated[MAX_TRANSLATED_SIZE - 1] = '\0';
    }

    context.current_turn.has_input = 1;

    /* Save current turn to circular buffer */
    memcpy(&context.turns[context.write_index], &context.current_turn, sizeof(turn_t));

    /* Advance write index (wrap around if at end) */
    context.write_index = (context.write_index + 1) % context.capacity;

    /* Update count (max out at capacity) */
    if (context.count < context.capacity) {
        context.count++;
    }

    /* Start fresh turn for next output */
    clear_turn(&context.current_turn);
}

int context_get_formatted(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0 || !context.turns) {
        return -1;
    }

    buffer[0] = '\0';
    size_t buffer_pos = 0;

    /*
     * Read turns from oldest to newest
     *
     * If buffer not full: start from index 0
     * If buffer full: start from write_index (oldest unoverwritten)
     */
    size_t start_index = (context.count < context.capacity) ? 0 : context.write_index;
    size_t turns_to_read = context.count;

    for (size_t i = 0; i < turns_to_read; i++) {
        /* Calculate actual index in circular buffer */
        size_t index = (start_index + i) % context.capacity;
        turn_t *turn = &context.turns[index];

        /* Only include turns that have user input (complete turns) */
        if (!turn->has_input) {
            continue;
        }

        /* Format: "Turn N Output: ...\nTurn N Input: ... (translated: ...)\n" */
        char turn_text[2048];
        int written;

        if (turn->translated[0] != '\0') {
            written = snprintf(turn_text, sizeof(turn_text),
                              "Turn %zu Output: %s\n"
                              "Turn %zu Input: %s (translated: %s)\n\n",
                              i + 1, turn->output,
                              i + 1, turn->user_input, turn->translated);
        } else {
            written = snprintf(turn_text, sizeof(turn_text),
                              "Turn %zu Output: %s\n"
                              "Turn %zu Input: %s\n\n",
                              i + 1, turn->output,
                              i + 1, turn->user_input);
        }

        /* Check if it fits in output buffer */
        if (buffer_pos + written >= buffer_size - 1) {
            /* Truncate */
            strncpy(buffer + buffer_pos, turn_text, buffer_size - buffer_pos - 1);
            buffer[buffer_size - 1] = '\0';
            fprintf(stderr, "Warning: Context truncated in formatting\n");
            return -1;
        }

        /* Append to buffer */
        strcpy(buffer + buffer_pos, turn_text);
        buffer_pos += written;
    }

    return 0;
}

void context_clear(void) {
    if (!context.turns) {
        return;
    }

    /* Reset all state */
    context.write_index = 0;
    context.count = 0;
    clear_turn(&context.current_turn);

    /* Zero out all turns */
    for (size_t i = 0; i < context.capacity; i++) {
        clear_turn(&context.turns[i]);
    }
}

size_t context_get_turn_count(void) {
    return context.count;
}

void context_shutdown(void) {
    if (context.turns) {
        free(context.turns);
        context.turns = NULL;
    }
    context.capacity = 0;
    context.write_index = 0;
    context.count = 0;
}
