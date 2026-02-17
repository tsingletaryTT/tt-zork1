/**
 * auto_player.c - Autonomous Player Implementation
 */

#include "auto_player.h"
#include "llm_router.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_initialized = 0;
static int g_enabled = 1;
static player_strategy_t g_strategy = STRATEGY_EXPLORE;

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

    fprintf(stderr, "Auto player: Initialized with %s strategy\n",
            strategy == STRATEGY_EXPLORE ? "explore" :
            strategy == STRATEGY_TREASURE ? "treasure" :
            strategy == STRATEGY_SURVIVAL ? "survival" : "speedrun");

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
 */
int auto_player_next_command(const char *game_state,
                             char *command_buffer,
                             size_t buffer_size) {
    if (!g_initialized || !g_enabled || !game_state) {
        return -1;
    }

    /* Build prompt based on strategy */
    char prompt[2048];
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
    }

    snprintf(prompt, sizeof(prompt),
             "Game state:\n%s\n\nStrategy: %s\nChoose next command:",
             game_state, strategy_hint);

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
