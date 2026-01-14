/**
 * monitor.h - Location Change Monitoring
 *
 * EDUCATIONAL PROJECT - Learn about system hooks and callbacks!
 *
 * This module monitors the Z-machine to detect when the player moves between
 * rooms. It hooks into the variable write system to catch changes to the
 * player's location (global variable 0).
 *
 * ## Architecture Context
 *
 * The monitor sits between the Z-machine interpreter and the journey tracker:
 *
 *     Z-machine writes variable → Monitor detects change → Tracker records
 *          (variable.c)              (monitor.c)            (tracker.c)
 *
 * ## Hook Pattern
 *
 * This is an example of the "Observer Pattern" or "Hook Pattern":
 * 1. Monitor registers a callback with the Z-machine
 * 2. Z-machine calls callback when location changes
 * 3. Callback extracts room info and records to tracker
 *
 * Benefits:
 * - Minimal changes to Z-machine code
 * - Clean separation of concerns
 * - Easy to enable/disable feature
 *
 * ## Global Variable 0
 *
 * In the Z-machine specification:
 * - Variables 0-15 are local/stack variables
 * - Variables 16+ are global variables
 * - Global variable 0 (variable #16) typically holds player location
 *
 * So when we see variable #16 change, the player moved!
 *
 * ## Direction Detection
 *
 * We detect directions in two ways:
 * 1. **Command parsing**: Look for "north", "south" etc. in user input
 * 2. **Fallback**: Use DIR_UNKNOWN for non-standard movements
 *
 * This gives us reasonable accuracy without complex analysis.
 */

#ifndef JOURNEY_MONITOR_H
#define JOURNEY_MONITOR_H

#include <stddef.h>

/* Forward declare zword from frotz */
#ifndef FROTZ_H_
typedef unsigned short zword;
#endif

/**
 * Initialize location monitoring
 *
 * Call this once at game startup. Sets up hooks into the Z-machine
 * variable system to detect location changes.
 *
 * @return 0 on success, -1 on error
 *
 * Example:
 *   if (monitor_init() != 0) {
 *       fprintf(stderr, "Failed to initialize location monitor\n");
 *   }
 *
 * Note: Safe to call multiple times (idempotent)
 */
int monitor_init(void);

/**
 * Callback: Location changed
 *
 * This is called by the Z-machine when global variable 0 changes.
 * Do NOT call this directly - it's a callback!
 *
 * @param old_location - Previous room object number
 * @param new_location - New room object number
 *
 * What this function does:
 * 1. Ignore if old_location == new_location (no actual move)
 * 2. Extract room name from Z-machine object
 * 3. Abbreviate room name
 * 4. Record move in journey tracker with current direction
 *
 * Thread safety: Not thread-safe (game is single-threaded)
 */
void monitor_location_changed(zword old_location, zword new_location);

/**
 * Set the direction for the next move
 *
 * Call this from input processing when a direction command is detected.
 * The direction will be used when the next location change is recorded.
 *
 * @param direction - Direction character (use DIR_* constants from tracker.h)
 *
 * Example:
 *   // In dinput.c when parsing "north"
 *   monitor_set_direction(DIR_NORTH);
 *
 * Note: Direction is consumed on next location change
 * If no direction is set, DIR_UNKNOWN is used
 */
void monitor_set_direction(char direction);

/**
 * Get the pending direction
 *
 * Returns the direction set by monitor_set_direction(), or DIR_UNKNOWN
 * if no direction has been set since the last location change.
 *
 * @return Current pending direction character
 *
 * Internal use mainly, but exposed for testing/debugging.
 */
char monitor_get_pending_direction(void);

/**
 * Enable or disable monitoring
 *
 * Allows turning the monitor on/off at runtime without reinitializing.
 * Useful for testing or performance tuning.
 *
 * @param enabled - 1 to enable, 0 to disable
 *
 * Example:
 *   monitor_set_enabled(0);  // Pause tracking temporarily
 *   // ... do something ...
 *   monitor_set_enabled(1);  // Resume tracking
 */
void monitor_set_enabled(int enabled);

/**
 * Check if monitoring is enabled
 *
 * @return 1 if enabled, 0 if disabled
 */
int monitor_is_enabled(void);

/**
 * Shutdown location monitoring
 *
 * Call this at game exit to clean up resources.
 * After calling this, must call monitor_init() again before monitoring.
 *
 * Example:
 *   monitor_shutdown();  // Clean exit
 */
void monitor_shutdown(void);

/**
 * Debug: Print monitor state
 *
 * Prints current monitoring state to stderr for debugging.
 * Shows whether monitoring is enabled and what the pending direction is.
 */
void monitor_debug_print(void);

#endif /* JOURNEY_MONITOR_H */
