/**
 * tracker.c - Journey Tracking Implementation
 *
 * EDUCATIONAL PROJECT - Heavily commented for learning!
 *
 * This implements the journey tracking system that records every room visited
 * during gameplay. The implementation uses a dynamic array that grows as needed.
 *
 * ## Implementation Strategy
 *
 * **Dynamic Array Growth**:
 * - Start with initial capacity (default: 50 steps)
 * - When full, double the capacity (realloc)
 * - This gives amortized O(1) append time
 *
 * **Memory Layout**:
 * ```
 * journey_history
 *   ├─ steps ──→ [step0][step1][step2]...[stepN]
 *   ├─ count (how many steps used)
 *   ├─ capacity (total allocated)
 *   └─ last_location (for quick change detection)
 * ```
 *
 * ## Why Not a Linked List?
 *
 * Pros of array:
 * - Better cache locality (steps stored contiguously)
 * - Simple iteration for map generation
 * - Less memory overhead (no next pointers)
 *
 * Cons of array:
 * - Occasional realloc (but rare with doubling strategy)
 * - Must copy data when growing
 *
 * For our use case, array wins!
 *
 * ## Error Handling Philosophy
 *
 * This code is non-critical (game works without maps), so:
 * - All errors return -1 but don't crash
 * - Game continues even if tracking fails
 * - Errors printed to stderr for debugging
 */

#include "tracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default capacity for initial allocation */
#define DEFAULT_INITIAL_CAPACITY 50

/* Maximum journey length (safety limit to prevent runaway memory) */
#define MAX_JOURNEY_LENGTH 1000

/* Global journey history (singleton pattern) */
static journey_history_t g_history = {
    .steps = NULL,
    .count = 0,
    .capacity = 0,
    .last_location = 0
};

/**
 * Helper: Grow the history array
 *
 * Doubles the capacity when we run out of space.
 * Uses realloc which may move the array to a new location.
 *
 * Returns: 0 on success, -1 on error
 */
static int grow_history(void) {
    size_t new_capacity = g_history.capacity * 2;

    /* Safety check: Don't grow beyond maximum */
    if (new_capacity > MAX_JOURNEY_LENGTH) {
        fprintf(stderr, "Journey tracking: Maximum length reached (%d steps)\n",
                MAX_JOURNEY_LENGTH);
        return -1;
    }

    /* Reallocate with doubled capacity */
    journey_step_t *new_steps = realloc(g_history.steps,
                                        new_capacity * sizeof(journey_step_t));
    if (!new_steps) {
        fprintf(stderr, "Journey tracking: Out of memory growing history\n");
        return -1;
    }

    g_history.steps = new_steps;
    g_history.capacity = new_capacity;

    fprintf(stderr, "Journey tracking: Grew capacity to %zu steps\n", new_capacity);
    return 0;
}

/*
 * Public API Implementation
 */

int journey_init(size_t initial_capacity) {
    /* Validate input */
    if (initial_capacity == 0) {
        initial_capacity = DEFAULT_INITIAL_CAPACITY;
    }

    if (initial_capacity > MAX_JOURNEY_LENGTH) {
        fprintf(stderr, "Journey tracking: Initial capacity too large, using %d\n",
                DEFAULT_INITIAL_CAPACITY);
        initial_capacity = DEFAULT_INITIAL_CAPACITY;
    }

    /* Allocate initial array */
    g_history.steps = calloc(initial_capacity, sizeof(journey_step_t));
    if (!g_history.steps) {
        fprintf(stderr, "Journey tracking: Out of memory during initialization\n");
        return -1;
    }

    g_history.capacity = initial_capacity;
    g_history.count = 0;
    g_history.last_location = 0;

    fprintf(stderr, "Journey tracking: Initialized (capacity: %zu)\n", initial_capacity);
    return 0;
}

int journey_record_move(zword room_obj, const char *room_name, char direction) {
    /* Validate inputs */
    if (!g_history.steps) {
        fprintf(stderr, "Journey tracking: Not initialized (call journey_init first)\n");
        return -1;
    }

    if (!room_name) {
        fprintf(stderr, "Journey tracking: NULL room name provided\n");
        return -1;
    }

    /* Check if we need to grow the array */
    if (g_history.count >= g_history.capacity) {
        if (grow_history() != 0) {
            return -1;  /* Growth failed */
        }
    }

    /* Record the new step */
    journey_step_t *step = &g_history.steps[g_history.count];
    step->room_obj = room_obj;
    step->direction = direction;
    step->sequence = g_history.count;

    /* Copy room name (truncate if too long) */
    strncpy(step->room_name, room_name, sizeof(step->room_name) - 1);
    step->room_name[sizeof(step->room_name) - 1] = '\0';

    /* Update tracking state */
    g_history.last_location = room_obj;
    g_history.count++;

    /* Debug output */
    fprintf(stderr, "Journey: Step %zu - %s (obj %d) via %c\n",
            g_history.count - 1, step->room_name, room_obj, direction);

    return 0;
}

journey_history_t *journey_get_history(void) {
    if (!g_history.steps) {
        return NULL;
    }
    return &g_history;
}

size_t journey_get_step_count(void) {
    return g_history.count;
}

zword journey_get_last_location(void) {
    return g_history.last_location;
}

void journey_clear(void) {
    if (!g_history.steps) {
        return;
    }

    /* Reset counters but keep allocated memory */
    g_history.count = 0;
    g_history.last_location = 0;

    /* Zero out the array (helps debugging) */
    memset(g_history.steps, 0, g_history.capacity * sizeof(journey_step_t));

    fprintf(stderr, "Journey tracking: Cleared history\n");
}

void journey_shutdown(void) {
    if (g_history.steps) {
        free(g_history.steps);
        g_history.steps = NULL;
    }

    g_history.count = 0;
    g_history.capacity = 0;
    g_history.last_location = 0;

    fprintf(stderr, "Journey tracking: Shutdown complete\n");
}

void journey_debug_print(void) {
    if (!g_history.steps || g_history.count == 0) {
        fprintf(stderr, "Journey: Empty\n");
        return;
    }

    fprintf(stderr, "\n=== Journey: %zu steps ===\n", g_history.count);
    for (size_t i = 0; i < g_history.count; i++) {
        journey_step_t *step = &g_history.steps[i];
        fprintf(stderr, "%3zu: %-20s (obj %5d) via %c\n",
                i, step->room_name, step->room_obj, step->direction);
    }
    fprintf(stderr, "========================\n\n");
}
