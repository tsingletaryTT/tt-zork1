/**
 * auto_player.c - Autonomous Player Implementation
 *
 * Supports two prompt modes:
 * 1. Original strategies (EXPLORE, TREASURE, SURVIVAL, SPEEDRUN)
 *    - Use router with system_v3_magic.txt prompt
 * 2. Persona strategies (EXPERT, NAIVE, COMPLETIONIST, EXPERIMENTAL)
 *    - Bypass router, call LLM client directly with persona prompts
 */

#include "auto_player.h"
#include "llm_router.h"
#include "llm_client.h"  /* Direct access for persona prompts */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>   /* For isspace, ispunct */
#include <unistd.h>  /* For usleep */

#define MAX_PROMPT_SIZE 16384  /* Maximum prompt file size (16KB) */

static int g_initialized = 0;
static int g_enabled = 1;
static player_strategy_t g_strategy = STRATEGY_EXPLORE;

/**
 * Extract clean command from LLM response
 *
 * Aggressively strips formatting and reasoning to extract clean Zork commands.
 *
 * Handles multiple patterns:
 * - <think> tags: "<think>reasoning</think> north" → "north"
 * - Markdown bold: "**Explore**,,examine mailbox" → "examine mailbox"
 * - Quoted commands: "I choose to \"go east\"" → "go east"
 * - Headers: "## My Command: north" → "north"
 * - Reasoning with commas: "Since I'm curious,,examine mailbox" → "examine mailbox"
 * - Colons as separators: "Command: examine mailbox" → "examine mailbox"
 */
static void clean_llm_response(const char *response, char *output, size_t output_size) {
    if (!response || !output || output_size == 0) {
        if (output && output_size > 0) output[0] = '\0';
        return;
    }

    /* Strategy: Look for quoted commands first (highest confidence) */
    const char *quote_start = strchr(response, '"');
    if (quote_start) {
        quote_start++; /* Skip opening quote */
        const char *quote_end = strchr(quote_start, '"');
        if (quote_end && (quote_end - quote_start) < output_size) {
            size_t len = quote_end - quote_start;
            strncpy(output, quote_start, len);
            output[len] = '\0';

            /* Success - we found a quoted command */
            /* Still trim whitespace */
            goto trim_output;
        }
    }

    /* Look for </think> tag */
    const char *think_end = strstr(response, "</think>");
    const char *start_pos = response;

    if (think_end) {
        /* Skip past the </think> tag */
        start_pos = think_end + strlen("</think>");
    }
    /* Look for <think> without closing tag */
    else if (strstr(response, "<think>")) {
        /* Use everything before <think> */
        const char *think_start = strstr(response, "<think>");
        size_t before_think = think_start - response;
        if (before_think > 0 && before_think < output_size) {
            strncpy(output, response, before_think);
            output[before_think] = '\0';
            goto trim_output;
        } else {
            output[0] = '\0';
            return;
        }
    }

    /* Copy from start_pos, will clean up formatting next */
    strncpy(output, start_pos, output_size - 1);
    output[output_size - 1] = '\0';

    /* PRIORITY 1: Extract text between code fences (```) if present */
    char *fence_start = strstr(output, "```");
    if (fence_start) {
        char *fence_end = strstr(fence_start + 3, "```");
        if (fence_end) {
            /* Found matching pair - extract content between them */
            fence_start += 3; /* Skip opening ``` */
            /* Skip optional language identifier (e.g., ```bash\n) */
            while (*fence_start && (*fence_start == '\n' || isalpha((unsigned char)*fence_start))) {
                fence_start++;
            }
            size_t len = fence_end - fence_start;
            if (len < output_size) {
                memmove(output, fence_start, len);
                output[len] = '\0';
                /* Successfully extracted fence content - skip rest of cleaning */
                goto trim_output;
            }
        }
    }

    /* PRIORITY 2: Extract text between **bold** markers if present */
    char *bold_start = strstr(output, "**");
    if (bold_start) {
        char *bold_end = strstr(bold_start + 2, "**");
        if (bold_end) {
            /* Found matching pair - extract content between them */
            bold_start += 2; /* Skip opening ** */
            size_t len = bold_end - bold_start;
            if (len < output_size) {
                memmove(output, bold_start, len);
                output[len] = '\0';
                /* Successfully extracted bold content - skip rest of cleaning */
                goto trim_output;
            }
        }
    }

    /* Remove markdown headers (##, ###, etc) */
    char *hash = output;
    while (*hash == '#' || *hash == ' ') hash++;
    if (hash != output) {
        memmove(output, hash, strlen(hash) + 1);
    }

    /* Remove all ** markers (bold formatting) */
    char *asterisk;
    while ((asterisk = strstr(output, "**")) != NULL) {
        memmove(asterisk, asterisk + 2, strlen(asterisk + 2) + 1);
    }

    /* Remove double-comma reasoning separators (,,) - take first part before ,, */
    char *double_comma = strstr(output, ",,");
    if (double_comma) {
        *double_comma = '\0'; /* Truncate at double comma */
    }

    /* Look for colon separator (e.g. "Command: examine mailbox") */
    /* But only if there's content after the colon */
    char *colon = strchr(output, ':');
    if (colon) {
        colon++; /* Skip the colon */
        while (*colon && isspace((unsigned char)*colon)) colon++;
        if (*colon) { /* Only use text after colon if it's not empty */
            memmove(output, colon, strlen(colon) + 1);
        }
    }

    /* Remove bullet points (-, *, •) at start */
    char *bullet = output;
    while (*bullet && (isspace((unsigned char)*bullet) ||
                       *bullet == '-' || *bullet == '*' ||
                       *bullet == '•' || *bullet == '>')) {
        bullet++;
    }
    if (bullet != output) {
        memmove(output, bullet, strlen(bullet) + 1);
    }

trim_output:
    /* Trim trailing whitespace and punctuation (but keep valid command chars) */
    char *end = output + strlen(output) - 1;
    while (end > output && (isspace((unsigned char)*end) ||
                            *end == ',' || *end == '.' ||
                            *end == ';' || *end == '!' ||
                            *end == '?')) {
        *end = '\0';
        end--;
    }

    /* Trim leading whitespace */
    char *start = output;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != output) {
        memmove(output, start, strlen(start) + 1);
    }

    /* Final sanity check: if output is too long (>100 chars), likely still has reasoning */
    if (strlen(output) > 100) {
        /* Take only first reasonable chunk (up to first sentence/comma/period) */
        char *terminator = output;
        int word_count = 0;
        while (*terminator && word_count < 5) {
            if (isspace((unsigned char)*terminator)) {
                word_count++;
            }
            terminator++;
        }
        /* Find next natural break after 5 words */
        while (*terminator && !isspace((unsigned char)*terminator)) {
            terminator++;
        }
        *terminator = '\0';
    }
}

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
        tui_output("Auto player: Failed to open prompt file: %s\n", filepath);
        return -1;
    }

    size_t bytes_read = fread(buffer, 1, buffer_size - 1, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    if (bytes_read == 0) {
        tui_output("Auto player: Empty prompt file: %s\n", filepath);
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
        tui_output("Auto player: Disabled via ZORK_PLAYER_ENABLED=0\n");
        return 0;
    }

    g_strategy = strategy;
    g_initialized = 1;

    tui_output("Auto player: Initialized with persona: %s\n",
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
        /* Persona strategies: Call LLM client directly with persona-specific prompts */
        const char *prompt_file = get_strategy_prompt_file(g_strategy);
        char system_prompt[MAX_PROMPT_SIZE];
        char user_prompt[2048];

        /* Load persona-specific system prompt */
        if (load_prompt_file(prompt_file, system_prompt, sizeof(system_prompt)) != 0) {
            tui_output("Auto player: Failed to load persona prompt: %s\n", prompt_file);
            return -1;
        }

        /* Build user prompt with game state */
        snprintf(user_prompt, sizeof(user_prompt),
                 "Current game state:\n%s\n\nWhat command do you choose?",
                 game_state);

        /* Configure LLM client for Player agent endpoint (port 8003) */
        const char *old_url = getenv("ZORK_LLM_URL");
        const char *old_model = getenv("ZORK_LLM_MODEL");

        setenv("ZORK_LLM_URL", "http://localhost:8003/v1/chat/completions", 1);
        setenv("ZORK_LLM_MODEL", "Llama-3.2-1B-Instruct", 1);

        /* Reinitialize client for player endpoint (quietly) */
        llm_client_set_quiet(1);
        llm_client_shutdown();
        llm_client_init();
        llm_client_set_quiet(0);

        /* Make direct API call with persona prompt */
        char raw_response[buffer_size];
        int result = llm_client_translate(system_prompt, user_prompt,
                                         raw_response, sizeof(raw_response), 10);

        /* Restore original environment */
        if (old_url) setenv("ZORK_LLM_URL", old_url, 1);
        if (old_model) setenv("ZORK_LLM_MODEL", old_model, 1);

        if (result != 0) {
            tui_output("Auto player: LLM request failed\n");
            return -1;
        }

        /* Clean up response (remove <think> tags and reasoning) */
        clean_llm_response(raw_response, command_buffer, buffer_size);

        /* Debug: Show what we got */
        #ifdef DEBUG_AUTO_PLAYER
        tui_output("[Auto-player DEBUG] Raw: %.100s...\n", raw_response);
        tui_output("[Auto-player DEBUG] Cleaned: %s\n", command_buffer);
        #endif

        /* Check if we got a valid command */
        if (strlen(command_buffer) == 0) {
            tui_output("Auto player: LLM returned empty command (raw: %.80s...)\n", raw_response);
            return -1;
        }

        return 0;

    } else {
        /* Original strategies: Use router with generic prompt */
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

        /* Call player agent via router */
        if (llm_router_request(LLM_TASK_AUTOPLAY, prompt,
                              command_buffer, buffer_size) != 0) {
            tui_output("Auto player: Failed to generate command\n");
            return -1;
        }

        return 0;
    }
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
