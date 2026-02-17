/**
 * narrative_enhancer.h - DM Agent for Narrative Enhancement
 *
 * The Dungeon Master (DM) agent adds dramatic flair and memorable moments
 * to the player's journey. It creates "postcards" - vivid descriptions of
 * key locations and events that are displayed alongside the journey map.
 *
 * ## Architecture
 *
 *   Game Events → DM Agent (Chip 2) → Enhanced Narratives
 *                      ↓
 *   Journey Tracker → Postcard Collection → Display at Game End
 *
 * ## "Postcards from Your Journey"
 *
 * The DM generates memorable moments that are saved as postcards:
 * - First location (opening scene)
 * - Milestone moments (finding treasures, dying, winning)
 * - Notable locations (revisited rooms, dangerous areas)
 *
 * At game end, postcards are displayed alongside the ASCII map to create
 * a complete visual and narrative summary of the player's adventure.
 *
 * ## LLM Integration
 *
 * Uses llm_router with LLM_TASK_NARRATE to route requests to Chip 2.
 * The DM uses creative temperature (0.9) for dramatic storytelling.
 */

#ifndef NARRATIVE_ENHANCER_H
#define NARRATIVE_ENHANCER_H

#include <stddef.h>

/**
 * Postcard - A memorable moment from the journey
 *
 * Each postcard captures a specific moment with enhanced narrative.
 * These are collected throughout the game and displayed at the end.
 */
typedef struct {
    char location[64];        /* Location name */
    char narrative[512];      /* DM-enhanced description */
    int step_number;          /* Journey step where this occurred */
    char moment_type[32];     /* "arrival", "treasure", "death", "victory" */
} postcard_t;

/**
 * Postcard Collection
 *
 * Stores all postcards generated during the journey.
 * Displayed alongside the ASCII map at game end.
 */
typedef struct {
    postcard_t *cards;        /* Array of postcards */
    size_t count;             /* Number of postcards */
    size_t capacity;          /* Allocated capacity */
} postcard_collection_t;

/**
 * Initialize narrative enhancer
 *
 * Sets up the DM agent and postcard collection system.
 * Call once at game startup.
 *
 * @return 0 on success, -1 on error
 */
int narrative_enhancer_init(void);

/**
 * Check if DM agent is enabled
 *
 * @return 1 if enabled, 0 if disabled
 */
int narrative_enhancer_is_enabled(void);

/**
 * Generate enhanced narrative for a location
 *
 * Calls DM agent (Chip 2) to create vivid description of current moment.
 * Optionally creates a postcard for key moments.
 *
 * @param location - Location name
 * @param context - Additional context (game state, events, etc.)
 * @param create_postcard - 1 to save as postcard, 0 for ephemeral narrative
 * @return 0 on success, -1 on error
 */
int narrative_enhancer_generate(const char *location,
                                 const char *context,
                                 int create_postcard);

/**
 * Create postcard for specific moment type
 *
 * Convenience function for common postcard types.
 *
 * @param location - Location name
 * @param moment_type - "arrival", "treasure", "death", "victory"
 * @param description - Base game description
 * @return 0 on success, -1 on error
 */
int narrative_enhancer_create_postcard(const char *location,
                                        const char *moment_type,
                                        const char *description);

/**
 * Get postcard collection
 *
 * Returns all postcards collected during the journey.
 * Used to display "Postcards from Your Journey" at game end.
 *
 * @return Pointer to collection, or NULL if not initialized
 */
postcard_collection_t *narrative_enhancer_get_postcards(void);

/**
 * Display postcards with journey map
 *
 * Shows all postcards alongside the ASCII map at game end.
 * Creates a complete visual and narrative summary of the adventure.
 */
void narrative_enhancer_display_postcards(void);

/**
 * Clear all postcards
 *
 * Resets collection for new game.
 */
void narrative_enhancer_clear(void);

/**
 * Shutdown narrative enhancer
 *
 * Frees all allocated memory. Call at game exit.
 */
void narrative_enhancer_shutdown(void);

#endif /* NARRATIVE_ENHANCER_H */
