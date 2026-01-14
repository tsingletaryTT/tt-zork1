/**
 * monitor.c - Location Change Monitoring Implementation
 *
 * EDUCATIONAL PROJECT - Heavily commented for learning!
 *
 * This implements the location monitoring system that hooks into the Z-machine
 * to detect when the player moves between rooms. It uses the Observer Pattern
 * to provide a clean interface between the Z-machine core and our journey
 * tracking feature.
 *
 * ## Implementation Strategy
 *
 * **State Machine**:
 * - Monitoring can be enabled/disabled at runtime
 * - Tracks "pending direction" - the direction user typed before move happens
 * - When location changes, combines direction + new location → journey step
 *
 * **Direction Tracking**:
 * ```
 * User types "north" → monitor_set_direction(DIR_NORTH)
 *                   ↓
 *          Store in pending_direction
 *                   ↓
 * Z-machine executes move → variable #16 changes
 *                   ↓
 *          monitor_location_changed() called
 *                   ↓
 *          Read pending_direction, record to tracker
 *                   ↓
 *          Reset pending_direction to DIR_UNKNOWN
 * ```
 *
 * ## Integration Points
 *
 * This module sits between:
 * - **variable.c**: Calls monitor_location_changed() when global var 0 changes
 * - **dinput.c**: Calls monitor_set_direction() when direction command detected
 * - **tracker.c**: We call journey_record_move() to record the step
 *
 * ## Why This Design?
 *
 * **Benefits of Observer Pattern**:
 * - Minimal changes to Z-machine code (just add one callback)
 * - Easy to enable/disable feature
 * - Clean separation: monitoring logic isolated here
 * - Testable independently
 *
 * **Alternative Considered**:
 * - Poll location every turn → wasteful, misses instant changes
 * - Hook into every move command → too many integration points
 * - This callback approach is the sweet spot!
 *
 * ## Room Name Extraction (Phase 2)
 *
 * Currently we use placeholder names like "Room#123".
 * Phase 2 will add proper room name extraction from Z-machine objects.
 */

#include "monitor.h"
#include "tracker.h"
#include "room_names.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Monitor state (singleton pattern) */
static struct {
    int enabled;              /* Is monitoring active? */
    char pending_direction;   /* Direction for next move */
    zword current_location;   /* Last known location */
    int initialized;          /* Has monitor_init() been called? */
} g_monitor = {
    .enabled = 1,             /* Enabled by default */
    .pending_direction = DIR_UNKNOWN,
    .current_location = 0,
    .initialized = 0
};

/* Room name extraction is now handled by room_names.c */

/*
 * Public API Implementation
 */

int monitor_init(void) {
    /* Safe to call multiple times (idempotent) */
    if (g_monitor.initialized) {
        fprintf(stderr, "Journey monitor: Already initialized\n");
        return 0;
    }

    /* Initialize journey tracker first */
    if (journey_init(50) != 0) {
        fprintf(stderr, "Journey monitor: Failed to initialize tracker\n");
        return -1;
    }

    /* Reset monitor state */
    g_monitor.enabled = 1;
    g_monitor.pending_direction = DIR_UNKNOWN;
    g_monitor.current_location = 0;
    g_monitor.initialized = 1;

    fprintf(stderr, "Journey monitor: Initialized\n");
    return 0;
}

void monitor_location_changed(zword old_location, zword new_location) {
    char abbrev_name[32];

    /* Validate state */
    if (!g_monitor.initialized) {
        fprintf(stderr, "Journey monitor: Not initialized (call monitor_init first)\n");
        return;
    }

    if (!g_monitor.enabled) {
        /* Monitoring disabled - ignore location changes */
        return;
    }

    /* Ignore non-changes (can happen with certain Z-machine operations) */
    if (old_location == new_location) {
        return;
    }

    /* Special case: First location (start of game)
     * Record it even though there was no "move" to get here
     */
    if (old_location == 0) {
        fprintf(stderr, "Journey monitor: Starting location detected (obj %d)\n",
                new_location);
        /* Keep DIR_UNKNOWN for first room (didn't "move" here) */
    }

    /* Extract and abbreviate room name from Z-machine object */
    if (room_get_abbrev_name(new_location, abbrev_name, sizeof(abbrev_name)) != 0) {
        /* Failed to get name - use fallback */
        snprintf(abbrev_name, sizeof(abbrev_name), "Room#%d", new_location);
    }

    /* Record the move to journey tracker */
    if (journey_record_move(new_location, abbrev_name, g_monitor.pending_direction) != 0) {
        fprintf(stderr, "Journey monitor: Failed to record move\n");
        /* Continue anyway - game should not crash if tracking fails */
    }

    /* Update state for next move */
    g_monitor.current_location = new_location;
    g_monitor.pending_direction = DIR_UNKNOWN;  /* Reset direction */

    /* Debug output */
    fprintf(stderr, "Journey monitor: Move recorded (obj %d → %d) via %c\n",
            old_location, new_location, g_monitor.pending_direction);
}

void monitor_set_direction(char direction) {
    if (!g_monitor.initialized) {
        fprintf(stderr, "Journey monitor: Not initialized (call monitor_init first)\n");
        return;
    }

    /* Validate direction character */
    const char *valid_directions = "NSEWUDIOnwes?";
    int valid = 0;
    for (const char *p = valid_directions; *p; p++) {
        if (*p == direction) {
            valid = 1;
            break;
        }
    }

    if (!valid) {
        fprintf(stderr, "Journey monitor: Invalid direction '%c', using DIR_UNKNOWN\n",
                direction);
        direction = DIR_UNKNOWN;
    }

    /* Store direction for next location change */
    g_monitor.pending_direction = direction;

    fprintf(stderr, "Journey monitor: Direction set to '%c'\n", direction);
}

char monitor_get_pending_direction(void) {
    return g_monitor.pending_direction;
}

void monitor_set_enabled(int enabled) {
    g_monitor.enabled = enabled ? 1 : 0;
    fprintf(stderr, "Journey monitor: %s\n",
            g_monitor.enabled ? "Enabled" : "Disabled");
}

int monitor_is_enabled(void) {
    return g_monitor.enabled;
}

void monitor_shutdown(void) {
    if (!g_monitor.initialized) {
        return;
    }

    /* Shutdown journey tracker */
    journey_shutdown();

    /* Reset monitor state */
    g_monitor.enabled = 0;
    g_monitor.pending_direction = DIR_UNKNOWN;
    g_monitor.current_location = 0;
    g_monitor.initialized = 0;

    fprintf(stderr, "Journey monitor: Shutdown complete\n");
}

void monitor_debug_print(void) {
    if (!g_monitor.initialized) {
        fprintf(stderr, "Journey monitor: Not initialized\n");
        return;
    }

    fprintf(stderr, "\n=== Journey Monitor State ===\n");
    fprintf(stderr, "Enabled: %s\n", g_monitor.enabled ? "YES" : "NO");
    fprintf(stderr, "Current location: %d\n", g_monitor.current_location);
    fprintf(stderr, "Pending direction: '%c'\n", g_monitor.pending_direction);
    fprintf(stderr, "===========================\n\n");

    /* Also print journey history */
    journey_debug_print();
}
