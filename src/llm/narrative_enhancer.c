/**
 * narrative_enhancer.c - DM Agent Implementation
 *
 * "Postcards from Your Journey" - Memorable moments with dramatic flair
 */

#include "narrative_enhancer.h"
#include "llm_router.h"
#include "../journey/tracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_POSTCARD_CAPACITY 20

/* Global state */
static postcard_collection_t g_postcards = {NULL, 0, 0};
static int g_initialized = 0;
static int g_enabled = 1;

/**
 * Extract postcard text from JSON response
 *
 * Looks for {"postcard": "..."} and extracts the narrative.
 * Falls back to raw response if JSON not found.
 */
static void extract_postcard_json(const char *raw_response, char *output, size_t output_size) {
    /* Look for "postcard": field */
    const char *postcard_start = strstr(raw_response, "\"postcard\":");
    if (postcard_start) {
        /* Find opening quote */
        postcard_start = strchr(postcard_start + 11, '"');
        if (postcard_start) {
            postcard_start++;  /* Skip quote */
            const char *postcard_end = strchr(postcard_start, '"');
            if (postcard_end) {
                size_t len = postcard_end - postcard_start;
                if (len < output_size) {
                    strncpy(output, postcard_start, len);
                    output[len] = '\0';
                    return;
                }
            }
        }
    }

    /* No JSON found, use raw response */
    strncpy(output, raw_response, output_size - 1);
    output[output_size - 1] = '\0';
}

/**
 * Initialize narrative enhancer
 */
int narrative_enhancer_init(void) {
    if (g_initialized) {
        return 0;  /* Already initialized */
    }

    /* Check if disabled via environment */
    const char *env_enabled = getenv("ZORK_DM_ENABLED");
    if (env_enabled && strcmp(env_enabled, "0") == 0) {
        g_enabled = 0;
        tui_output("Narrative enhancer: Disabled via ZORK_DM_ENABLED=0\n");
        return 0;
    }

    /* Allocate initial postcard storage */
    g_postcards.cards = calloc(INITIAL_POSTCARD_CAPACITY, sizeof(postcard_t));
    if (!g_postcards.cards) {
        tui_output("Narrative enhancer: Out of memory\n");
        return -1;
    }

    g_postcards.capacity = INITIAL_POSTCARD_CAPACITY;
    g_postcards.count = 0;
    g_initialized = 1;

    tui_output("Narrative enhancer: Initialized\n");
    return 0;
}

/**
 * Check if DM agent is enabled
 */
int narrative_enhancer_is_enabled(void) {
    return g_initialized && g_enabled;
}

/**
 * Add postcard to collection
 */
static int add_postcard(const char *location,
                        const char *narrative,
                        const char *moment_type) {
    /* Grow array if needed */
    if (g_postcards.count >= g_postcards.capacity) {
        size_t new_capacity = g_postcards.capacity * 2;
        postcard_t *new_cards = realloc(g_postcards.cards,
                                        new_capacity * sizeof(postcard_t));
        if (!new_cards) {
            tui_output("Narrative enhancer: Failed to grow postcard array\n");
            return -1;
        }
        g_postcards.cards = new_cards;
        g_postcards.capacity = new_capacity;
    }

    /* Add new postcard */
    postcard_t *card = &g_postcards.cards[g_postcards.count];
    strncpy(card->location, location, sizeof(card->location) - 1);
    card->location[sizeof(card->location) - 1] = '\0';

    strncpy(card->narrative, narrative, sizeof(card->narrative) - 1);
    card->narrative[sizeof(card->narrative) - 1] = '\0';

    strncpy(card->moment_type, moment_type, sizeof(card->moment_type) - 1);
    card->moment_type[sizeof(card->moment_type) - 1] = '\0';

    card->step_number = journey_get_step_count();

    g_postcards.count++;
    return 0;
}

/**
 * Generate enhanced narrative for a location
 */
int narrative_enhancer_generate(const char *location,
                                 const char *context,
                                 int create_postcard) {
    if (!g_initialized || !g_enabled) {
        return 0;  /* Silently skip if disabled */
    }

    if (!location) {
        return -1;
    }

    /* Build prompt for DM agent */
    char prompt[1024];
    snprintf(prompt, sizeof(prompt),
             "Location: %s\nContext: %s\n"
             "Create a vivid, dramatic description (2-3 sentences) of this moment.",
             location, context ? context : "");

    /* Call DM via router */
    char raw_response[512];
    if (llm_router_request(LLM_TASK_NARRATE, prompt, raw_response, sizeof(raw_response)) != 0) {
        tui_output("Narrative enhancer: Failed to generate narrative for %s\n",
                location);
        return -1;
    }

    /* Extract postcard from JSON */
    char narrative[512];
    extract_postcard_json(raw_response, narrative, sizeof(narrative));

    /* Display narrative */
    printf("\n📖 %s\n", narrative);

    /* Create postcard if requested */
    if (create_postcard) {
        return add_postcard(location, narrative, "moment");
    }

    return 0;
}

/**
 * Create postcard for specific moment type
 */
int narrative_enhancer_create_postcard(const char *location,
                                        const char *moment_type,
                                        const char *description) {
    if (!g_initialized || !g_enabled) {
        return 0;
    }

    if (!location || !moment_type || !description) {
        return -1;
    }

    /* Build prompt for DM agent */
    char prompt[1024];
    snprintf(prompt, sizeof(prompt),
             "Location: %s, Moment: %s\nDescription: %s",
             location, moment_type, description);

    /* Call DM via router */
    char raw_response[512];
    if (llm_router_request(LLM_TASK_NARRATE, prompt, raw_response, sizeof(raw_response)) != 0) {
        tui_output("Narrative enhancer: Failed to create postcard for %s\n",
                location);
        return -1;
    }

    /* Extract postcard from JSON */
    char narrative[512];
    extract_postcard_json(raw_response, narrative, sizeof(narrative));

    /* Add to collection */
    return add_postcard(location, narrative, moment_type);
}

/**
 * Get postcard collection
 */
postcard_collection_t *narrative_enhancer_get_postcards(void) {
    if (!g_initialized) {
        return NULL;
    }
    return &g_postcards;
}

/**
 * Display postcards with journey map
 */
void narrative_enhancer_display_postcards(void) {
    if (!g_initialized || g_postcards.count == 0) {
        return;
    }

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║          📬 POSTCARDS FROM YOUR JOURNEY                   ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    for (size_t i = 0; i < g_postcards.count; i++) {
        postcard_t *card = &g_postcards.cards[i];

        printf("┌────────────────────────────────────────────────────────┐\n");
        printf("│ 📍 %s%-45s │\n", card->location,
               "                                             ");
        printf("│ Step %d%-49d │\n", card->step_number,
               0);  /* Padding hack */
        printf("├────────────────────────────────────────────────────────┤\n");

        /* Word-wrap narrative to fit in box */
        const char *text = card->narrative;
        char line[57];  /* 56 chars + null */
        while (*text) {
            size_t len = 0;
            const char *word_start = text;

            /* Fill line with words */
            while (*text && len < 56) {
                if (*text == ' ' || *text == '\n') {
                    if (len > 0) {
                        word_start = text + 1;
                    }
                    text++;
                } else {
                    text++;
                    len++;
                }
            }

            /* If we didn't fit the whole string, backtrack to last word boundary */
            if (*text && text > word_start) {
                text = word_start;
                len = text - (text - len);
            }

            /* Copy line */
            size_t line_len = text - (text - len);
            if (line_len > 56) line_len = 56;
            memcpy(line, text - len, line_len);
            line[line_len] = '\0';

            printf("│ %s%-54s │\n", line, "                                                      ");

            if (!*text) break;
        }

        printf("└────────────────────────────────────────────────────────┘\n");
        printf("\n");
    }

    printf("Total postcards collected: %zu\n", g_postcards.count);
    printf("\n");
}

/**
 * Clear all postcards
 */
void narrative_enhancer_clear(void) {
    if (g_initialized) {
        g_postcards.count = 0;
    }
}

/**
 * Shutdown narrative enhancer
 */
void narrative_enhancer_shutdown(void) {
    if (g_initialized) {
        free(g_postcards.cards);
        g_postcards.cards = NULL;
        g_postcards.count = 0;
        g_postcards.capacity = 0;
        g_initialized = 0;
    }
}
