/**
 * auto_player.h - Autonomous Player Agent
 *
 * The Player agent (Chip 3) can play Zork autonomously by selecting
 * commands based on game state and exploration strategy.
 *
 * ## Use Cases
 *
 * 1. **AI vs AI gameplay** - Watch two agents play against each other
 * 2. **Automated testing** - Test game paths and scenarios
 * 3. **Demonstration mode** - Show off the game
 * 4. **Speedrunning** - Find optimal paths
 *
 * ## Architecture
 *
 *   Game State → Player Agent (Chip 3) → Command Selection
 *                      ↓
 *              Strategy: explore, solve, survive
 *
 * ## Integration with Translator
 *
 * Player agent generates natural language commands which are then
 * processed by the Translator agent, demonstrating full multi-agent
 * collaboration.
 */

#ifndef AUTO_PLAYER_H
#define AUTO_PLAYER_H

#include <stddef.h>

/**
 * Player strategy types
 *
 * Original strategies (use generic magic prompt):
 * - STRATEGY_EXPLORE, STRATEGY_TREASURE, STRATEGY_SURVIVAL, STRATEGY_SPEEDRUN
 *
 * Persona-based strategies (use specialized prompts with different knowledge levels):
 * - STRATEGY_EXPERT: Full Zork knowledge, speedrun optimization
 * - STRATEGY_NAIVE: No prior Zork knowledge, learns through play
 * - STRATEGY_COMPLETIONIST: Methodical 100% completion approach
 * - STRATEGY_EXPERIMENTAL: Tests boundaries, creative approaches
 */
typedef enum {
    /* Original strategies - use system_v3_magic.txt prompt */
    STRATEGY_EXPLORE,          /* Systematic exploration of all rooms */
    STRATEGY_TREASURE,         /* Focus on finding treasures */
    STRATEGY_SURVIVAL,         /* Avoid danger, conservative play */
    STRATEGY_SPEEDRUN,         /* Optimal path to win condition */

    /* Persona-based strategies - use specialized prompt files */
    STRATEGY_EXPERT,           /* Expert speedrunner - knows everything, plays to win */
    STRATEGY_NAIVE,            /* Naive explorer - knows nothing, learns by doing */
    STRATEGY_COMPLETIONIST,    /* Completionist - seeks all treasures and best endings */
    STRATEGY_EXPERIMENTAL      /* Experimental - tests boundaries, tries unusual approaches */
} player_strategy_t;

/**
 * Initialize autonomous player
 *
 * @param strategy - Initial strategy to use
 * @return 0 on success, -1 on error
 */
int auto_player_init(player_strategy_t strategy);

/**
 * Check if player agent is enabled
 *
 * @return 1 if enabled, 0 if disabled
 */
int auto_player_is_enabled(void);

/**
 * Generate next command based on game state
 *
 * @param game_state - Current game output/description
 * @param command_buffer - Output buffer for generated command
 * @param buffer_size - Size of command buffer
 * @return 0 on success, -1 on error
 */
int auto_player_next_command(const char *game_state,
                             char *command_buffer,
                             size_t buffer_size);

/**
 * Set player strategy
 *
 * @param strategy - New strategy to use
 */
void auto_player_set_strategy(player_strategy_t strategy);

/**
 * Get current strategy
 *
 * @return Current strategy
 */
player_strategy_t auto_player_get_strategy(void);

/**
 * Get persona name for display
 *
 * @param strategy - Strategy to get name for
 * @return Human-readable persona name
 */
const char *auto_player_get_persona_name(player_strategy_t strategy);

/**
 * Shutdown autonomous player
 */
void auto_player_shutdown(void);

#endif /* AUTO_PLAYER_H */
