/**
 * scene_visualizer.c - ASCII Art Generation Implementation
 *
 * This module brings the game world to life with ASCII art!
 */

#include "scene_visualizer.h"
#include "llm_router.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Configuration */
static struct {
    int initialized;
    int enabled;
    char last_location[128];  /* Cache to avoid regenerating same location */
} visualizer_state = {0};

/* Buffer sizes */
#define ART_BUFFER_SIZE 4096
#define PROMPT_BUFFER_SIZE 2048

/**
 * Initialize scene visualizer
 */
int scene_visualizer_init(void) {
    /* Check if artist is enabled */
    const char *enabled_env = getenv("ZORK_ARTIST_ENABLED");
    if (enabled_env && strcmp(enabled_env, "0") == 0) {
        visualizer_state.enabled = 0;
        visualizer_state.initialized = 1;
        tui_output("Scene visualizer: Disabled\n");
        return -1;
    }

    /* Check if router is ready */
    if (!llm_router_is_ready()) {
        tui_output("Warning: Scene visualizer disabled (router not ready)\n");
        visualizer_state.enabled = 0;
        visualizer_state.initialized = 1;
        return -1;
    }

    visualizer_state.enabled = 1;
    visualizer_state.initialized = 1;
    visualizer_state.last_location[0] = '\0';

    tui_output("Scene visualizer: Initialized\n");
    return 0;
}

/**
 * Generate ASCII art for location
 */
int scene_visualizer_generate(const char *location_name, const char *description) {
    /* Validate */
    if (!visualizer_state.initialized || !visualizer_state.enabled) {
        return -1;
    }

    if (!location_name || !description) {
        return -1;
    }

    /* Skip if same location as last time (avoid redundant generation) */
    if (strcmp(visualizer_state.last_location, location_name) == 0) {
        return 0;
    }

    /* Build prompt for artist - keep it simple to match system prompt pattern */
    char prompt[PROMPT_BUFFER_SIZE];
    snprintf(prompt, sizeof(prompt), "%s:", location_name);

    /* Call artist via router */
    char raw_response[ART_BUFFER_SIZE];
    if (llm_router_request(LLM_TASK_VISUALIZE, prompt, raw_response, sizeof(raw_response)) != 0) {
        tui_output("Warning: Failed to generate art for %s: %s\n",
                location_name, llm_router_get_last_error());
        return -1;
    }

    /* Extract art from JSON response if present */
    char art[ART_BUFFER_SIZE];
    const char *art_start = strstr(raw_response, "\"art\":");
    if (art_start) {
        /* Find the opening quote after "art": */
        art_start = strchr(art_start + 6, '"');
        if (art_start) {
            art_start++; /* Skip opening quote */
            const char *art_end = strchr(art_start, '"');
            if (art_end) {
                size_t len = art_end - art_start;
                if (len < sizeof(art)) {
                    strncpy(art, art_start, len);
                    art[len] = '\0';
                    /* Replace \\n with actual newlines */
                    char *pos = art;
                    while ((pos = strstr(pos, "\\n")) != NULL) {
                        *pos = '\n';
                        memmove(pos + 1, pos + 2, strlen(pos + 2) + 1);
                    }
                } else {
                    strcpy(art, "Art too large");
                }
            } else {
                /* No JSON, use raw response */
                strncpy(art, raw_response, sizeof(art) - 1);
                art[sizeof(art) - 1] = '\0';
            }
        } else {
            strncpy(art, raw_response, sizeof(art) - 1);
            art[sizeof(art) - 1] = '\0';
        }
    } else {
        /* No JSON structure, use raw response */
        strncpy(art, raw_response, sizeof(art) - 1);
        art[sizeof(art) - 1] = '\0';
    }

    /* Display art in a bordered box */
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  🎨 %s%-47s ║\n", location_name,
           "                                                    ");
    printf("╠════════════════════════════════════════════════════════════╣\n");

    /* Display art lines */
    char *line = art;
    char *next_line;
    int line_count = 0;
    const int max_lines = 10;

    while (line && *line && line_count < max_lines) {
        next_line = strchr(line, '\n');
        if (next_line) {
            *next_line = '\0';
            next_line++;
        }

        /* Trim line and display */
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        if (*trimmed) {
            printf("║  %-58s║\n", trimmed);
            line_count++;
        }

        line = next_line;
    }

    /* Fill remaining lines if needed */
    while (line_count < 6) {
        printf("║                                                            ║\n");
        line_count++;
    }

    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    fflush(stdout);

    /* Cache location to avoid regenerating */
    strncpy(visualizer_state.last_location, location_name,
            sizeof(visualizer_state.last_location) - 1);

    return 0;
}

/**
 * Check if enabled
 */
int scene_visualizer_is_enabled(void) {
    return visualizer_state.enabled;
}

/**
 * Shutdown
 */
void scene_visualizer_shutdown(void) {
    visualizer_state.initialized = 0;
    visualizer_state.enabled = 0;
    visualizer_state.last_location[0] = '\0';
}
