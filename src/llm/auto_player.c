/**
 * auto_player.c - Autonomous Player Implementation
 *
 * Supports two prompt modes:
 * 1. Original strategies (EXPLORE, TREASURE, SURVIVAL, SPEEDRUN)
 *    - Use system_v3_magic.txt with simple strategy hints
 * 2. Persona strategies (EXPERT, NAIVE, COMPLETIONIST, EXPERIMENTAL)
 *    - Use specialized prompt files with distinct knowledge levels
 */

#include "auto_player.h"
#include "llm_router.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PROMPT_SIZE 16384  /* Maximum prompt file size (16KB) */

static int g_initialized = 0;
static int g_enabled = 1;
static player_strategy_t g_strategy = STRATEGY_EXPLORE;

/**
 * Get prompt file path for strategy
 *
 * Original strategies use the generic magic prompt.
 * Persona strategies use specialized prompt files.
 */
static const char *get_strategy_prompt_file(player_strategy_t strategy) {
    switch (strategy) {
        case STRATEGY_EXPERT:
            return "prompts/player/expert_speedrunner.txt";
        case STRATEGY_NAIVE:
            return "prompts/player/naive_explorer.txt";
        case STRATEGY_COMPLETIONIST:
            return "prompts/player/completionist.txt";
        case STRATEGY_EXPERIMENTAL:
            return "prompts/player/experimental.txt";
        default:
            /* Original strategies use generic magic prompt */
            return "prompts/player/system_v3_magic.txt";
    }
}

/**
 * Load prompt from file
 *
 * @param filepath Path to prompt file
 * @param buffer Output buffer for prompt content
 * @param buffer_size Size of output buffer
 * @return 0 on success, -1 on error
 */
static int load_prompt_file(const char *filepath, char *buffer, size_t buffer_size) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        fprintf(stderr, "Auto player: Failed to open prompt file: %s\n", filepath);
        return -1;
    }

    size_t bytes_read = fread(buffer, 1, buffer_size - 1, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    if (bytes_read == 0) {
        fprintf(stderr, "Auto player: Empty prompt file: %s\n", filepath);
        return -1;
    }

    return 0;
}

/**
 * Get persona name for display
 */
const char *auto_player_get_persona_name(player_strategy_t strategy) {
    switch (strategy) {
        case STRATEGY_EXPLORE:
            return "Explorer (generic)";
        case STRATEGY_TREASURE:
            return "Treasure Hunter (generic)";
        case STRATEGY_SURVIVAL:
            return "Survivalist (generic)";
        case STRATEGY_SPEEDRUN:
            return "Speedrunner (generic)";
        case STRATEGY_EXPERT:
            return "Expert Speedrunner";
        case STRATEGY_NAIVE:
            return "Naive Explorer";
        case STRATEGY_COMPLETIONIST:
            return "Completionist";
        case STRATEGY_EXPERIMENTAL:
            return "Experimental";
        default:
            return "Unknown";
    }
}

/**
 * Initialize autonomous player
 */
int auto_player_init(player_strategy_t strategy) {
    if (g_initialized) {
        return 0;
    }

    /* Check if disabled */
    const char *env_enabled = getenv("ZORK_PLAYER_ENABLED");
    if (env_enabled && strcmp(env_enabled, "0") == 0) {
        g_enabled = 0;
        fprintf(stderr, "Auto player: Disabled via ZORK_PLAYER_ENABLED=0\n");
        return 0;
    }

    g_strategy = strategy;
    g_initialized = 1;

    fprintf(stderr, "Auto player: Initialized with persona: %s\n",
            auto_player_get_persona_name(strategy));

    return 0;
}

/**
 * Check if enabled
 */
int auto_player_is_enabled(void) {
    return g_initialized && g_enabled;
}

/**
 * Generate next command
 *
 * For persona strategies (EXPERT, NAIVE, COMPLETIONIST, EXPERIMENTAL):
 *   - Load specialized prompt from file
 *   - Append current game state
 *   - Send to player agent
 *
 * For original strategies (EXPLORE, TREASURE, SURVIVAL, SPEEDRUN):
 *   - Use generic magic prompt with simple strategy hint
 */
int auto_player_next_command(const char *game_state,
                             char *command_buffer,
                             size_t buffer_size) {
    if (!g_initialized || !g_enabled || !game_state) {
        return -1;
    }

    char prompt[MAX_PROMPT_SIZE];

    /* Check if this is a persona strategy */
    if (g_strategy >= STRATEGY_EXPERT) {
        /* Load persona-specific prompt from file */
        const char *prompt_file = get_strategy_prompt_file(g_strategy);
        char persona_prompt[MAX_PROMPT_SIZE];

        if (load_prompt_file(prompt_file, persona_prompt, sizeof(persona_prompt)) != 0) {
            fprintf(stderr, "Auto player: Failed to load prompt, using fallback\n");

            /* Fallback to simple prompt */
            snprintf(prompt, sizeof(prompt),
                     "You are an autonomous Zork player.\n\n"
                     "Game state:\n%s\n\n"
                     "Output ONLY a single valid Zork command:",
                     game_state);
        } else {
            /* Append game state to persona prompt */
            snprintf(prompt, sizeof(prompt),
                     "%s\n\nCurrent game state:\n%s\n",
                     persona_prompt, game_state);
        }
    } else {
        /* Original strategies - use simple hints */
        const char *strategy_hint = "";

        switch (g_strategy) {
            case STRATEGY_EXPLORE:
                strategy_hint = "Explore systematically. Try new directions.";
                break;
            case STRATEGY_TREASURE:
                strategy_hint = "Look for treasures and valuable items.";
                break;
            case STRATEGY_SURVIVAL:
                strategy_hint = "Be cautious. Avoid dark places without light.";
                break;
            case STRATEGY_SPEEDRUN:
                strategy_hint = "Take the fastest path to victory.";
                break;
            default:
                strategy_hint = "Play intelligently.";
                break;
        }

        snprintf(prompt, sizeof(prompt),
                 "Game state:\n%s\n\nStrategy: %s\nChoose next command:",
                 game_state, strategy_hint);
    }

    /* Call player agent via router */
    if (llm_router_request(LLM_TASK_AUTOPLAY, prompt,
                          command_buffer, buffer_size) != 0) {
        fprintf(stderr, "Auto player: Failed to generate command\n");
        return -1;
    }

    return 0;
}

/**
 * Set strategy
 */
void auto_player_set_strategy(player_strategy_t strategy) {
    g_strategy = strategy;
}

/**
 * Get strategy
 */
player_strategy_t auto_player_get_strategy(void) {
    return g_strategy;
}

/**
 * Shutdown
 */
void auto_player_shutdown(void) {
    g_initialized = 0;
}
