/**
 * scene_visualizer.h - ASCII Art Generation for Game Locations
 *
 * This module generates ASCII art visualizations of game locations using
 * the Artist LLM agent (Chip 1). It hooks into location changes and creates
 * atmospheric ASCII art based on room descriptions.
 *
 * ## How It Works
 *
 * 1. Journey monitor detects location change
 * 2. Scene visualizer captures room description
 * 3. Calls llm_router with LLM_TASK_VISUALIZE
 * 4. Artist agent (Chip 1) generates ASCII art
 * 5. Display art above game text
 *
 * ## Integration Points
 *
 * - Journey monitor: Triggers on location change
 * - Output capture: Gets room descriptions
 * - LLM router: Routes to artist endpoint (port 8001)
 *
 * ## Example
 *
 * ```
 * Location: West of House
 * Description: You are standing in an open field west of a white house...
 *
 * → Artist generates:
 *
 * ╔════════════════════════════════════════════════════╗
 * ║           🏚️  West of House                        ║
 * ╠════════════════════════════════════════════════════╣
 * ║                                                    ║
 * ║     🌳                    🏠                        ║
 * ║    🌲🌳          📬        🏠🏠                       ║
 * ║   🌳🌲🌳                  🏠🏠🏠                       ║
 * ║                                                    ║
 * ╚════════════════════════════════════════════════════╝
 * ```
 */

#ifndef TT_ZORK_SCENE_VISUALIZER_H
#define TT_ZORK_SCENE_VISUALIZER_H

#include <stddef.h>

/**
 * Initialize scene visualizer
 *
 * Sets up hooks for location change detection.
 *
 * @return 0 on success, -1 on error (not critical, game continues)
 */
int scene_visualizer_init(void);

/**
 * Generate ASCII art for current location
 *
 * This is called when location changes. It:
 * 1. Captures the room description
 * 2. Calls artist agent via llm_router
 * 3. Displays the ASCII art
 *
 * @param location_name Name of the location (e.g., "West of House")
 * @param description Full room description text
 * @return 0 on success, -1 if art generation failed (not critical)
 */
int scene_visualizer_generate(const char *location_name, const char *description);

/**
 * Check if visualizer is enabled
 *
 * Can be disabled via ZORK_ARTIST_ENABLED=0 environment variable.
 *
 * @return 1 if enabled, 0 if disabled
 */
int scene_visualizer_is_enabled(void);

/**
 * Shut down scene visualizer
 */
void scene_visualizer_shutdown(void);

#endif /* TT_ZORK_SCENE_VISUALIZER_H */
