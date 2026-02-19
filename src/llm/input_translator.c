/**
 * input_translator.c - Translation Orchestrator Implementation
 *
 * EDUCATIONAL PROJECT - See how everything connects!
 *
 * This module is the "conductor" that coordinates all other modules.
 * It's a great example of how to build a complex system from simple pieces.
 *
 * ## Design Pattern: Facade
 *
 * This implements the "Facade Pattern" - providing a simple interface to
 * a complex subsystem. Instead of callers needing to know about context
 * managers, prompt loaders, JSON builders, HTTP clients, etc., they just
 * call one function: translator_process().
 *
 * ## Initialization Order (Critical!)
 *
 * Modules must be initialized in the right order because of dependencies:
 *
 * 1. Context manager (no dependencies)
 * 2. Output capture (needs context manager)
 * 3. Prompt loader (no dependencies)
 * 4. LLM client (no dependencies)
 *
 * If you change the order, things will break!
 *
 * ## Error Propagation Strategy
 *
 * Each module can fail independently:
 * - Context manager fails → no history, but LLM still works
 * - Prompt loader fails → use defaults, LLM still works
 * - LLM client fails → fall back to original input
 *
 * This is called "graceful degradation" - the system gets worse, not broken.
 *
 * ## Statistics Tracking
 *
 * We track translation stats for debugging:
 * - How many times was LLM called?
 * - How often did it succeed?
 * - How often did we fall back?
 *
 * This helps diagnose issues:
 * - 100% fallbacks → LLM server is down
 * - 50% fallbacks → Flaky network or bad prompts
 * - 0% fallbacks → Everything working perfectly!
 */

#include "input_translator.h"
#include "context.h"
#include "output_capture.h"
#include "prompt_loader.h"
#include "llm_client.h"
#include "llm_router.h"
#include "tui_output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Buffer sizes */
#define CONTEXT_BUFFER_SIZE (32 * 1024)  /* 32KB for game context */
#define PROMPT_BUFFER_SIZE (64 * 1024)   /* 64KB for formatted prompts */
#define COMMAND_BUFFER_SIZE 512          /* 512B for translated commands */

/* Statistics */
static struct {
    int initialized;
    int enabled;
    int total_translations;
    int successful_translations;
    int fallbacks;
} stats = {0};

/* Maximum number of turns to track (configurable)
 * NOTE: Reduced to 3 for small models like Qwen2.5:0.5b
 * Too much context confuses small models - they try to "complete" previous actions
 * Larger models can handle 10-20 turns
 */
#define DEFAULT_MAX_TURNS 3

int translator_init(void) {
    tui_output("\n=== Initializing LLM Translation System ===\n");

    /* Initialize context manager */
    tui_output("[1/4] Context manager...\n");
    if (context_init(DEFAULT_MAX_TURNS) != 0) {
        tui_output("Warning: Context manager init failed\n");
        /* Continue - not fatal */
    }

    /* Initialize output capture */
    tui_output("[2/4] Output capture...\n");
    if (output_capture_init() != 0) {
        tui_output("Warning: Output capture init failed\n");
        /* Continue - not fatal */
    }

    /* Initialize prompt loader */
    tui_output("[3/4] Prompt loader...\n");
    if (prompt_loader_init("prompts") != 0) {
        tui_output("Info: Using default prompts\n");
        /* Continue - defaults are fine */
    }

    /* Initialize LLM router (replaces direct client) */
    tui_output("[4/4] LLM router...\n");
    if (llm_router_init(NULL) != 0) {
        tui_output("Warning: LLM router unavailable - translation disabled\n");
        tui_output("  Error: %s\n", llm_router_get_last_error());
        stats.enabled = 0;
        stats.initialized = 1;
        tui_output("=== LLM system initialized (DISABLED - fallback mode) ===\n\n");
        return -1;
    }

    /* Check router mode */
    LLMMode mode = llm_router_get_mode();
    tui_output("  Mode: %s\n", llm_router_mode_to_string(mode));

    /* Check if LLM is ready */
    if (!llm_router_is_ready()) {
        tui_output("Info: LLM router not ready\n");
        stats.enabled = 0;
    } else {
        stats.enabled = 1;
    }

    stats.initialized = 1;
    tui_output("=== LLM system initialized (%s) ===\n\n",
            stats.enabled ? "ENABLED" : "DISABLED");

    return stats.enabled ? 0 : -1;
}

int translator_process(const char *user_input,
                        char *output_commands,
                        size_t output_size) {
    /* Validate parameters */
    if (!user_input || !output_commands || output_size == 0) {
        return -1;
    }

    /* Auto-initialize if needed */
    if (!stats.initialized) {
        translator_init();
    }

    /* Count this attempt */
    stats.total_translations++;

    /* Check if LLM is enabled */
    if (!stats.enabled || !llm_router_is_ready()) {
        /* Fallback: pass through original input */
        strncpy(output_commands, user_input, output_size - 1);
        output_commands[output_size - 1] = '\0';
        stats.fallbacks++;
        return -1;
    }

    /*
     * Step 1: Call LLM Router (TRANSLATE task)
     *
     * The router handles:
     * - Mode detection (multi-agent vs unified)
     * - Endpoint selection (translator endpoint in multi-agent mode)
     * - Prompt loading (from config files)
     * - API communication
     *
     * Much simpler than the old approach!
     */
    char llm_response[COMMAND_BUFFER_SIZE];
    if (llm_router_request(LLM_TASK_TRANSLATE, user_input, llm_response,
                            sizeof(llm_response)) != 0) {
        fprintf(stderr, "Warning: LLM translation failed: %s\n",
                llm_router_get_last_error());
        goto fallback;
    }

    /* Check if response is empty */
    if (llm_response[0] == '\0') {
        fprintf(stderr, "Warning: LLM returned empty response\n");
        goto fallback;
    }

    /*
     * Step 4: Display translation to user
     *
     * Show what the LLM decided. This is educational - the user sees
     * how their natural language was translated.
     */
    fprintf(stdout, "\n[LLM → %s]\n\n", llm_response);
    fflush(stdout);

    /*
     * Step 5: Record in context
     *
     * Save this interaction so future translations have more context.
     */
    context_add_input(user_input, llm_response);

    /*
     * Step 6: Return translated commands
     *
     * Copy to output buffer. The caller will send this to the Z-machine.
     */
    strncpy(output_commands, llm_response, output_size - 1);
    output_commands[output_size - 1] = '\0';

    stats.successful_translations++;
    return 0;

fallback:
    /*
     * Fallback path: Pass through original input
     *
     * If anything went wrong, we just use what the user typed.
     * This ensures the game is always playable.
     */
    strncpy(output_commands, user_input, output_size - 1);
    output_commands[output_size - 1] = '\0';
    stats.fallbacks++;
    return -1;
}

int translator_is_enabled(void) {
    if (!stats.initialized) {
        translator_init();
    }
    return stats.enabled && llm_client_is_enabled();
}

void translator_get_stats(int *total_translations,
                           int *successful,
                           int *fallbacks) {
    if (total_translations) *total_translations = stats.total_translations;
    if (successful) *successful = stats.successful_translations;
    if (fallbacks) *fallbacks = stats.fallbacks;
}

void translator_shutdown(void) {
    if (!stats.initialized) {
        return;
    }

    /* Print statistics */
    fprintf(stderr, "\n=== Translation Statistics ===\n");
    fprintf(stderr, "Total attempts:  %d\n", stats.total_translations);
    fprintf(stderr, "Successful:      %d\n", stats.successful_translations);
    fprintf(stderr, "Fallbacks:       %d\n", stats.fallbacks);

    if (stats.total_translations > 0) {
        int success_rate = (stats.successful_translations * 100) / stats.total_translations;
        fprintf(stderr, "Success rate:    %d%%\n", success_rate);
    }
    fprintf(stderr, "==============================\n\n");

    /* Shutdown subsystems (in reverse order of initialization) */
    llm_router_shutdown();
    llm_client_shutdown();
    prompt_loader_shutdown();
    output_capture_shutdown();
    context_shutdown();

    stats.initialized = 0;
}
