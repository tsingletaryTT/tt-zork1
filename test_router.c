/**
 * test_router.c - LLM Router Test Program
 *
 * Tests the flexible LLM router with different modes and task types.
 *
 * Compile:
 *   gcc -o test_router test_router.c \
 *       src/llm/llm_router.c \
 *       src/llm/llm_client.c \
 *       src/llm/prompt_loader.c \
 *       src/llm/json_helper.c \
 *       -Isrc/llm -lcurl
 *
 * Usage:
 *   ./test_router [config_file]
 *
 * Configuration is read from config/llm_mode.yaml (or specified file).
 * Make sure servers are running before testing!
 */

#include <stdio.h>
#include <string.h>
#include "llm_router.h"

/* ANSI color codes */
#define COLOR_GREEN  "\033[0;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_RED    "\033[0;31m"
#define COLOR_CYAN   "\033[0;36m"
#define COLOR_RESET  "\033[0m"

/**
 * Test inputs for translator
 */
static const char *test_inputs[] = {
    "open the mailbox",
    "go north",
    "take the brass lamp",
    "look around",
    "read the leaflet",
    NULL
};

/**
 * Test a single request
 */
static int test_request(LLMTaskType task, const char *input) {
    char output[512] = {0};

    printf(COLOR_YELLOW "Input:" COLOR_RESET " %s\n", input);

    int result = llm_router_request(task, input, output, sizeof(output));

    if (result == 0) {
        printf(COLOR_GREEN "Output:" COLOR_RESET " %s\n", output);
        return 1;
    } else {
        printf(COLOR_RED "Error:" COLOR_RESET " %s\n",
               llm_router_get_last_error());
        return 0;
    }
}

/**
 * Test translator task
 */
static void test_translator(void) {
    printf("\n" COLOR_CYAN "=== Testing TRANSLATE Task ===" COLOR_RESET "\n\n");

    int success = 0;
    int total = 0;

    for (int i = 0; test_inputs[i] != NULL; i++) {
        if (test_request(LLM_TASK_TRANSLATE, test_inputs[i])) {
            success++;
        }
        total++;
        printf("\n");
    }

    printf(COLOR_CYAN "Results:" COLOR_RESET " %d/%d successful (%.1f%%)\n",
           success, total, (float)success * 100.0 / total);
}

/**
 * Display router info
 */
static void show_router_info(void) {
    LLMMode mode = llm_router_get_mode();

    printf(COLOR_GREEN "=== LLM Router Information ===" COLOR_RESET "\n\n");
    printf("Mode: %s\n", llm_router_mode_to_string(mode));
    printf("Status: %s\n\n",
           llm_router_is_ready() ? "Ready" : "Not initialized");

    /* Show configuration for each task */
    const char *task_names[] = {"TRANSLATE", "VISUALIZE", "NARRATE", "AUTOPLAY"};

    for (int i = 0; i < 4; i++) {
        char info[512];
        if (llm_router_get_task_info((LLMTaskType)i, info, sizeof(info)) == 0) {
            printf(COLOR_CYAN "%s Configuration:" COLOR_RESET "\n", task_names[i]);
            printf("%s\n\n", info);
        }
    }
}

/**
 * Main test program
 */
int main(int argc, char **argv) {
    const char *config_file = (argc > 1) ? argv[1] : NULL;

    printf(COLOR_GREEN "╔═══════════════════════════════════════════╗\n");
    printf("║     LLM ROUTER TEST PROGRAM             ║\n");
    printf("╚═══════════════════════════════════════════╝" COLOR_RESET "\n\n");

    /* Initialize router */
    printf("Initializing router...\n");
    if (llm_router_init(config_file) != 0) {
        fprintf(stderr, COLOR_RED "Failed to initialize router: %s\n" COLOR_RESET,
                llm_router_get_last_error());
        return 1;
    }

    printf(COLOR_GREEN "✓ Router initialized\n" COLOR_RESET);

    /* Show configuration */
    show_router_info();

    /* Test translator */
    test_translator();

    /* TODO: Add tests for other task types when implemented:
     * - test_visualizer()
     * - test_narrator()
     * - test_autoplay()
     */

    /* Cleanup */
    printf("\n" COLOR_GREEN "=== Test Complete ===" COLOR_RESET "\n");
    llm_router_shutdown();

    return 0;
}
