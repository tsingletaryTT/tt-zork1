/**
 * slash_commands.h - In-Game Command System
 *
 * Provides slash commands for controlling LLM features and gameplay modes
 * during a Zork session without restarting the game.
 *
 * ## Available Commands
 *
 * `/mode classic` - Pure Zork (no LLM features)
 * `/mode enhanced` - All LLM features enabled
 * `/play solo` - Human input (with translation if enhanced)
 * `/play auto` - AI player takes over
 * `/help` - Show available commands
 * `/status` - Show current settings
 *
 * ## Architecture
 *
 * Slash commands are intercepted in the input layer before being
 * sent to the Z-machine interpreter. They control flags that
 * enable/disable LLM features dynamically.
 */

#ifndef SLASH_COMMANDS_H
#define SLASH_COMMANDS_H

/**
 * Game modes
 */
typedef enum {
    MODE_CLASSIC,    /* Pure Zork - no LLM features */
    MODE_ENHANCED    /* All LLM features enabled */
} game_mode_t;

/**
 * Play modes
 */
typedef enum {
    PLAY_SOLO,       /* Human player */
    PLAY_AUTO        /* AI player */
} play_mode_t;

/**
 * Slash command result
 */
typedef struct {
    int handled;           /* 1 if command was handled, 0 if not a slash command */
    int should_skip;       /* 1 if game turn should be skipped (command was internal) */
    char message[256];     /* Message to display to user */
} slash_result_t;

/**
 * Initialize slash command system
 *
 * @param initial_game_mode - Starting game mode
 * @param initial_play_mode - Starting play mode
 */
void slash_commands_init(game_mode_t initial_game_mode,
                         play_mode_t initial_play_mode);

/**
 * Process potential slash command
 *
 * @param input - User input to check
 * @return Result structure indicating if command was handled
 */
slash_result_t slash_commands_process(const char *input);

/**
 * Get current game mode
 *
 * @return Current game mode (classic or enhanced)
 */
game_mode_t slash_commands_get_game_mode(void);

/**
 * Get current play mode
 *
 * @return Current play mode (solo or auto)
 */
play_mode_t slash_commands_get_play_mode(void);

/**
 * Check if translator should be used
 *
 * @return 1 if translator enabled, 0 otherwise
 */
int slash_commands_use_translator(void);

/**
 * Check if artist should be used
 *
 * @return 1 if artist enabled, 0 otherwise
 */
int slash_commands_use_artist(void);

/**
 * Check if DM should be used
 *
 * @return 1 if DM enabled, 0 otherwise
 */
int slash_commands_use_dm(void);

/**
 * Check if auto-play is enabled
 *
 * @return 1 if auto-play, 0 if human
 */
int slash_commands_is_auto_play(void);

/**
 * Get help text
 *
 * @return String with all available slash commands
 */
const char *slash_commands_get_help(void);

/**
 * Get status text
 *
 * @return String showing current mode settings
 */
const char *slash_commands_get_status(void);

#endif /* SLASH_COMMANDS_H */
