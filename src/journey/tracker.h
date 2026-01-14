/**
 * tracker.h - Journey Tracking System
 *
 * EDUCATIONAL PROJECT - Learn about game state tracking!
 *
 * This module implements a journey tracking system that records every room
 * the player visits during their playthrough of Zork. The data is used to
 * generate an ASCII map showing the player's complete path when they die
 * or win the game.
 *
 * ## Architecture Context
 *
 * The tracker sits between the Z-machine interpreter and the map generator:
 *
 *     Z-machine → Location Monitor → Journey Tracker → Map Generator
 *                                         ↓
 *                                   History Storage
 *
 * ## Key Concepts
 *
 * **Journey Step**: A single room visit with direction taken to get there
 * **Journey History**: Complete sequence of all steps taken
 * **Circular Buffer**: Fixed-size array that wraps around (for memory efficiency)
 *
 * ## Data Structure Design
 *
 * We use a dynamic array that grows as needed (realloc). Alternatives:
 * - Linked list: More memory per node, but O(1) append
 * - Fixed array: Simple but may run out of space
 * - We chose dynamic array: Best balance of simplicity and efficiency
 *
 * ## Memory Management
 *
 * Total memory per step: ~50 bytes (room_obj + name + direction + sequence)
 * For 100 steps: ~5KB (very reasonable!)
 * Maximum history: Configurable (default 200 steps)
 *
 * ## Thread Safety
 *
 * NOT thread-safe! Game is single-threaded so this is fine.
 * If you add threading later, add mutexes around history modifications.
 */

#ifndef JOURNEY_TRACKER_H
#define JOURNEY_TRACKER_H

#include <stddef.h>

/* Forward declare zword from frotz */
#ifndef FROTZ_H_
typedef unsigned short zword;
#endif

/**
 * Direction constants
 *
 * These represent the cardinal directions and special movements in the game.
 * Used to record how the player moved from one room to another.
 */
#define DIR_NORTH    'N'
#define DIR_SOUTH    'S'
#define DIR_EAST     'E'
#define DIR_WEST     'W'
#define DIR_UP       'U'
#define DIR_DOWN     'D'
#define DIR_IN       'I'
#define DIR_OUT      'O'
#define DIR_NORTHEAST 'n'
#define DIR_NORTHWEST 'w'
#define DIR_SOUTHEAST 'e'
#define DIR_SOUTHWEST 's'
#define DIR_UNKNOWN  '?'  /* For teleportation or non-standard moves */

/**
 * Journey Step
 *
 * Represents a single visited location in the player's journey.
 * This is the fundamental unit of tracking.
 *
 * Example:
 *   room_obj = 142 (Z-machine object number for "West of House")
 *   room_name = "W.House"
 *   direction = 'N' (came from south)
 *   sequence = 1 (first step)
 */
typedef struct {
    zword room_obj;           /* Z-machine object number for this room */
    char room_name[32];       /* Abbreviated room name for display */
    char direction;           /* Direction taken to arrive here */
    int sequence;             /* Visit sequence number (0-based) */
} journey_step_t;

/**
 * Journey History
 *
 * Complete record of the player's path through the game.
 * This is the main data structure that holds all visited locations.
 *
 * The steps array is dynamically allocated and grows as needed.
 */
typedef struct {
    journey_step_t *steps;    /* Array of visited locations */
    size_t count;             /* Number of steps recorded */
    size_t capacity;          /* Allocated capacity (for growth) */
    zword last_location;      /* Last known room object (for change detection) */
} journey_history_t;

/**
 * Initialize journey tracking
 *
 * Call this once at game startup before any tracking occurs.
 * Allocates initial storage for the journey history.
 *
 * @param initial_capacity - Initial size of history array (default: 50)
 * @return 0 on success, -1 on error (out of memory)
 *
 * Example:
 *   if (journey_init(100) != 0) {
 *       fprintf(stderr, "Failed to initialize journey tracking\n");
 *   }
 */
int journey_init(size_t initial_capacity);

/**
 * Record a movement to a new location
 *
 * Call this whenever the player moves to a different room.
 * Automatically grows the history array if needed.
 *
 * @param room_obj - Z-machine object number for the new room
 * @param room_name - Abbreviated name of the room
 * @param direction - Direction taken (use DIR_* constants)
 * @return 0 on success, -1 on error (out of memory)
 *
 * Example:
 *   journey_record_move(142, "W.House", DIR_NORTH);
 *
 * Notes:
 * - Duplicate rooms are allowed (shows revisits on map)
 * - If direction is unknown, use DIR_UNKNOWN
 * - room_name is copied, so caller can free original
 */
int journey_record_move(zword room_obj, const char *room_name, char direction);

/**
 * Get the complete journey history
 *
 * Returns a pointer to the history structure for reading.
 * Do NOT modify the returned structure directly!
 *
 * @return Pointer to journey history (NULL if not initialized)
 *
 * Example:
 *   journey_history_t *history = journey_get_history();
 *   if (history) {
 *       printf("Visited %zu rooms\n", history->count);
 *       for (size_t i = 0; i < history->count; i++) {
 *           printf("%d: %s\n", i, history->steps[i].room_name);
 *       }
 *   }
 */
journey_history_t *journey_get_history(void);

/**
 * Get the current step count
 *
 * Convenience function to get number of recorded steps without
 * accessing the history structure directly.
 *
 * @return Number of steps recorded, or 0 if not initialized
 */
size_t journey_get_step_count(void);

/**
 * Get the last recorded location
 *
 * Useful for detecting when the player has moved to a new room.
 *
 * @return Z-machine object number of last recorded room, or 0 if empty
 */
zword journey_get_last_location(void);

/**
 * Clear all recorded journey data
 *
 * Resets the journey history without deallocating memory.
 * Use this to start tracking a new playthrough.
 *
 * Example:
 *   journey_clear();  // Start fresh for new game
 */
void journey_clear(void);

/**
 * Shutdown journey tracking
 *
 * Call this at game exit to free all allocated memory.
 * After calling this, must call journey_init() again before tracking.
 *
 * Example:
 *   journey_shutdown();  // Clean exit
 */
void journey_shutdown(void);

/**
 * Debug: Print journey to stderr
 *
 * Useful for development and debugging. Prints the complete journey
 * in a human-readable format to stderr.
 *
 * Example output:
 *   Journey: 5 steps
 *   0: W.House (via ?)
 *   1: N.House (via N)
 *   2: Forest (via N)
 *   3: N.House (via S)
 *   4: W.House (via S)
 */
void journey_debug_print(void);

#endif /* JOURNEY_TRACKER_H */
