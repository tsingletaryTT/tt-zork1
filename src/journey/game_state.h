/**
 * game_state.h - Game End State Detection
 *
 * EDUCATIONAL PROJECT - Learn about game state detection!
 *
 * This module detects when and why a game ends (death, victory, or user quit).
 * It monitors game output and state to determine if the player should see
 * their journey map.
 *
 * ## Detection Strategy
 *
 * **Text Pattern Matching**:
 * We watch for specific output patterns that indicate game ending:
 * - Death: "You have died", "****  You have died  ****", "You are dead"
 * - Victory: "You have won", "Congratulations!", "****  You have won  ****"
 * - User Quit: User types "quit" command
 *
 * ## Why This Approach?
 *
 * Alternative approaches considered:
 * 1. **Check game variables**: Too game-specific, varies by title
 * 2. **Monitor score**: Not reliable (deaths can happen without score change)
 * 3. **Text patterns**: Universal across IF games, reliable
 *
 * ## Integration Points
 *
 * - Called from z_quit() in process.c before termination
 * - Monitors text output through game_state_watch_output()
 * - Journey map generation triggered only for death/victory
 *
 * ## Example Flow
 *
 * ```
 * Player dies:
 *   Game prints "****  You have died  ****"
 *   → game_state_watch_output() sees death pattern
 *   → Sets internal state to DEATH
 *   Game calls z_quit()
 *   → game_state_should_show_map() returns TRUE
 *   → Map is displayed!
 *
 * Player types "quit":
 *   → game_state_set_user_quit() called
 *   Game calls z_quit()
 *   → game_state_should_show_map() returns FALSE
 *   → No map (as requested by user)
 * ```
 */

#ifndef JOURNEY_GAME_STATE_H
#define JOURNEY_GAME_STATE_H

/**
 * Game end reason
 */
typedef enum {
    GAME_END_UNKNOWN = 0,  /* Haven't determined yet */
    GAME_END_DEATH,        /* Player died */
    GAME_END_VICTORY,      /* Player won */
    GAME_END_USER_QUIT     /* User quit intentionally */
} game_end_reason_t;

/**
 * Initialize game state detection
 *
 * Call this once at game startup.
 *
 * @return 0 on success, -1 on error
 */
int game_state_init(void);

/**
 * Watch game text output for end state patterns
 *
 * Call this as text is being output to the player.
 * Detects death/victory messages.
 *
 * @param text - Text being displayed to player
 *
 * Example patterns detected:
 * - "****  You have died  ****" → DEATH
 * - "****  You have won  ****" → VICTORY
 * - "You are dead" → DEATH
 */
void game_state_watch_output(const char *text);

/**
 * Mark that user intentionally quit
 *
 * Call this when user types "quit" command.
 * Prevents map from showing for intentional quits.
 */
void game_state_set_user_quit(void);

/**
 * Check if journey map should be shown
 *
 * Returns TRUE for death/victory, FALSE for user quit.
 *
 * @return 1 if map should be shown, 0 if not
 *
 * Example:
 *   if (game_state_should_show_map()) {
 *       map_generate_and_display();
 *   }
 */
int game_state_should_show_map(void);

/**
 * Get the reason game ended
 *
 * @return Game end reason
 */
game_end_reason_t game_state_get_reason(void);

/**
 * Get string description of end reason
 *
 * @return Human-readable description
 */
const char *game_state_get_reason_string(void);

/**
 * Reset game state for new game
 *
 * Call this if player restarts.
 */
void game_state_reset(void);

/**
 * Shutdown game state detection
 */
void game_state_shutdown(void);

#endif /* JOURNEY_GAME_STATE_H */
