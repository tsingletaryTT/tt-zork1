/**
 * slash_commands.c - In-Game Command System Implementation
 */

#include "slash_commands.h"
#include "auto_player.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Global state */
static game_mode_t g_game_mode = MODE_ENHANCED;
static play_mode_t g_play_mode = PLAY_SOLO;
static int g_initialized = 0;

/**
 * Initialize slash command system
 */
void slash_commands_init(game_mode_t initial_game_mode,
                         play_mode_t initial_play_mode) {
    g_game_mode = initial_game_mode;
    g_play_mode = initial_play_mode;
    g_initialized = 1;
}

/**
 * Trim whitespace from string
 */
static void trim(char *str) {
    char *end;

    /* Trim leading space */
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return;

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
}

/**
 * Convert string to lowercase
 */
static void to_lower(char *str) {
    for (char *p = str; *p; p++) {
        *p = tolower((unsigned char)*p);
    }
}

/**
 * Process potential slash command
 */
slash_result_t slash_commands_process(const char *input) {
    slash_result_t result = {0, 0, ""};

    if (!g_initialized || !input || input[0] != '/') {
        return result;  /* Not a slash command */
    }

    /* Copy and normalize input */
    char cmd[256];
    strncpy(cmd, input, sizeof(cmd) - 1);
    cmd[sizeof(cmd) - 1] = '\0';
    trim(cmd);
    to_lower(cmd);

    result.handled = 1;
    result.should_skip = 1;  /* Most commands don't send input to game */

    /* Parse command */
    if (strcmp(cmd, "/help") == 0 || strcmp(cmd, "/?") == 0) {
        snprintf(result.message, sizeof(result.message),
                 "\n=== Slash Commands ===\n\n"
                 "Game Modes:\n"
                 "  /mode classic   - Pure Zork (no LLM features)\n"
                 "  /mode enhanced  - All LLM features enabled\n\n"
                 "Play Modes:\n"
                 "  /play solo      - Human player (you type commands)\n"
                 "  /play auto      - AI player takes over\n\n"
                 "Player Personas (auto mode only):\n"
                 "  /player expert        - Speedrunner (knows everything)\n"
                 "  /player naive         - Explorer (knows nothing)\n"
                 "  /player completionist - 100%% completion hunter\n"
                 "  /player experimental  - Boundary tester\n\n"
                 "Information:\n"
                 "  /status         - Show current settings\n"
                 "  /help           - Show this help\n\n"
                 "LLM Features (enhanced mode):\n"
                 "  - Natural language translation (Chip 0)\n"
                 "  - ASCII art generation (Chip 1)\n"
                 "  - Journey postcards (Chip 2)\n"
                 "  - Autonomous player (Chip 3)\n");

    } else if (strncmp(cmd, "/mode ", 6) == 0) {
        const char *mode = cmd + 6;
        trim((char *)mode);

        if (strcmp(mode, "classic") == 0) {
            g_game_mode = MODE_CLASSIC;
            snprintf(result.message, sizeof(result.message),
                     "\n✓ Game mode: CLASSIC\n"
                     "  LLM features disabled. Pure Zork experience!\n");
        } else if (strcmp(mode, "enhanced") == 0) {
            g_game_mode = MODE_ENHANCED;
            snprintf(result.message, sizeof(result.message),
                     "\n✓ Game mode: ENHANCED\n"
                     "  LLM features enabled:\n"
                     "  - Natural language translation\n"
                     "  - ASCII art for locations\n"
                     "  - Journey postcards\n");
        } else {
            snprintf(result.message, sizeof(result.message),
                     "\n❌ Unknown mode: %s\n"
                     "Valid modes: classic, enhanced\n"
                     "Type /help for more info.\n", mode);
        }

    } else if (strncmp(cmd, "/play ", 6) == 0) {
        const char *mode = cmd + 6;
        trim((char *)mode);

        if (strcmp(mode, "solo") == 0) {
            g_play_mode = PLAY_SOLO;
            snprintf(result.message, sizeof(result.message),
                     "\n✓ Play mode: SOLO\n"
                     "  You are in control. Type commands to play.\n");
        } else if (strcmp(mode, "auto") == 0) {
            g_play_mode = PLAY_AUTO;
            if (g_game_mode == MODE_CLASSIC) {
                snprintf(result.message, sizeof(result.message),
                         "\n⚠️  Auto-play requires enhanced mode!\n"
                         "  Switching to enhanced mode...\n");
                g_game_mode = MODE_ENHANCED;
            } else {
                snprintf(result.message, sizeof(result.message),
                         "\n✓ Play mode: AUTO\n"
                         "  AI player (Chip 3) taking over!\n"
                         "  Watch as the agent explores Zork.\n"
                         "  Type /play solo to resume manual control.\n");
            }
        } else {
            snprintf(result.message, sizeof(result.message),
                     "\n❌ Unknown play mode: %s\n"
                     "Valid modes: solo, auto\n"
                     "Type /help for more info.\n", mode);
        }

    } else if (strcmp(cmd, "/status") == 0) {
        const char *game_mode_str = (g_game_mode == MODE_CLASSIC) ? "CLASSIC" : "ENHANCED";
        const char *play_mode_str = (g_play_mode == PLAY_SOLO) ? "SOLO" : "AUTO";
        const char *persona_str = auto_player_get_persona_name(auto_player_get_strategy());

        snprintf(result.message, sizeof(result.message),
                 "\n=== Current Settings ===\n\n"
                 "Game Mode: %s\n"
                 "Play Mode: %s\n"
                 "Player Persona: %s\n\n"
                 "Active Features:\n"
                 "  %s Natural language translation (Chip 0)\n"
                 "  %s ASCII art generation (Chip 1)\n"
                 "  %s Journey postcards (Chip 2)\n"
                 "  %s Autonomous player (Chip 3)\n\n"
                 "Type /help for available commands.\n",
                 game_mode_str, play_mode_str, persona_str,
                 (g_game_mode == MODE_ENHANCED) ? "✓" : "✗",
                 (g_game_mode == MODE_ENHANCED) ? "✓" : "✗",
                 (g_game_mode == MODE_ENHANCED) ? "✓" : "✗",
                 (g_play_mode == PLAY_AUTO && g_game_mode == MODE_ENHANCED) ? "✓" : "✗");

    } else if (strcmp(cmd, "/mode") == 0) {
        snprintf(result.message, sizeof(result.message),
                 "\nUsage: /mode <classic|enhanced>\n"
                 "Type /help for more info.\n");

    } else if (strcmp(cmd, "/play") == 0) {
        snprintf(result.message, sizeof(result.message),
                 "\nUsage: /play <solo|auto>\n"
                 "Type /help for more info.\n");

    } else if (strncmp(cmd, "/player ", 8) == 0) {
        const char *persona = cmd + 8;
        trim((char *)persona);

        if (strcmp(persona, "expert") == 0) {
            auto_player_set_strategy(STRATEGY_EXPERT);
            snprintf(result.message, sizeof(result.message),
                     "\n✓ Player persona: EXPERT SPEEDRUNNER\n"
                     "  Full Zork knowledge, optimal play, plays to win!\n"
                     "  • Knows all treasure locations\n"
                     "  • Memorized optimal paths\n"
                     "  • Lamp battery management mastery\n");
        } else if (strcmp(persona, "naive") == 0) {
            auto_player_set_strategy(STRATEGY_NAIVE);
            snprintf(result.message, sizeof(result.message),
                     "\n✓ Player persona: NAIVE EXPLORER\n"
                     "  No prior Zork knowledge, learns by doing!\n"
                     "  • Discovers everything naturally\n"
                     "  • Curious and cautious\n"
                     "  • Makes beginner mistakes\n");
        } else if (strcmp(persona, "completionist") == 0) {
            auto_player_set_strategy(STRATEGY_COMPLETIONIST);
            snprintf(result.message, sizeof(result.message),
                     "\n✓ Player persona: COMPLETIONIST\n"
                     "  Seeks all treasures and best endings!\n"
                     "  • Methodical 100%% completion\n"
                     "  • Maps every location\n"
                     "  • Collects all 20 treasures\n");
        } else if (strcmp(persona, "experimental") == 0) {
            auto_player_set_strategy(STRATEGY_EXPERIMENTAL);
            snprintf(result.message, sizeof(result.message),
                     "\n✓ Player persona: EXPERIMENTAL\n"
                     "  Tests boundaries, tries unusual approaches!\n"
                     "  • Creative problem-solving\n"
                     "  • Tests parser limits\n"
                     "  • Discovers unintended mechanics\n");
        } else {
            snprintf(result.message, sizeof(result.message),
                     "\n❌ Unknown persona: %s\n\n"
                     "Valid personas:\n"
                     "  expert         - Speedrunner (knows everything)\n"
                     "  naive          - Explorer (knows nothing)\n"
                     "  completionist  - 100%% completion hunter\n"
                     "  experimental   - Boundary tester\n\n"
                     "Type /help for more info.\n", persona);
        }

    } else if (strcmp(cmd, "/player") == 0) {
        snprintf(result.message, sizeof(result.message),
                 "\nUsage: /player <persona>\n\n"
                 "Valid personas:\n"
                 "  expert         - Speedrunner (knows everything)\n"
                 "  naive          - Explorer (knows nothing)\n"
                 "  completionist  - 100%% completion hunter\n"
                 "  experimental   - Boundary tester\n\n"
                 "Type /help for more info.\n");

    } else {
        snprintf(result.message, sizeof(result.message),
                 "\n❌ Unknown command: %s\n"
                 "Type /help for available commands.\n", cmd);
    }

    return result;
}

/**
 * Get current game mode
 */
game_mode_t slash_commands_get_game_mode(void) {
    return g_game_mode;
}

/**
 * Get current play mode
 */
play_mode_t slash_commands_get_play_mode(void) {
    return g_play_mode;
}

/**
 * Check if translator should be used
 */
int slash_commands_use_translator(void) {
    return g_game_mode == MODE_ENHANCED && g_play_mode == PLAY_SOLO;
}

/**
 * Check if artist should be used
 */
int slash_commands_use_artist(void) {
    return g_game_mode == MODE_ENHANCED;
}

/**
 * Check if DM should be used
 */
int slash_commands_use_dm(void) {
    return g_game_mode == MODE_ENHANCED;
}

/**
 * Check if auto-play is enabled
 */
int slash_commands_is_auto_play(void) {
    return g_play_mode == PLAY_AUTO && g_game_mode == MODE_ENHANCED;
}

/**
 * Get help text
 */
const char *slash_commands_get_help(void) {
    return "\n=== Slash Commands ===\n\n"
           "Game Modes:\n"
           "  /mode classic   - Pure Zork (no LLM features)\n"
           "  /mode enhanced  - All LLM features enabled\n\n"
           "Play Modes:\n"
           "  /play solo      - Human player\n"
           "  /play auto      - AI player\n\n"
           "Player Personas (auto mode only):\n"
           "  /player expert        - Speedrunner (knows everything)\n"
           "  /player naive         - Explorer (knows nothing)\n"
           "  /player completionist - 100% completion hunter\n"
           "  /player experimental  - Boundary tester\n\n"
           "Information:\n"
           "  /status         - Show current settings\n"
           "  /help           - Show this help\n";
}

/**
 * Get status text
 */
const char *slash_commands_get_status(void) {
    static char status[512];
    const char *game_mode_str = (g_game_mode == MODE_CLASSIC) ? "CLASSIC" : "ENHANCED";
    const char *play_mode_str = (g_play_mode == PLAY_SOLO) ? "SOLO" : "AUTO";
    const char *persona_str = auto_player_get_persona_name(auto_player_get_strategy());

    snprintf(status, sizeof(status),
             "\n=== Current Settings ===\n"
             "Game Mode: %s\n"
             "Play Mode: %s\n"
             "Player Persona: %s\n",
             game_mode_str, play_mode_str, persona_str);

    return status;
}
