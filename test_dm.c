#include <stdio.h>
#include "src/llm/llm_router.h"
#include "src/llm/narrative_enhancer.h"

int main() {
    printf("=== Testing DM Agent (Chip 2) ===\n\n");

    /* Initialize router */
    if (llm_router_init("config/llm_mode.yaml") != 0) {
        fprintf(stderr, "Router init failed: %s\n", llm_router_get_last_error());
        return 1;
    }

    printf("✅ Router initialized in %s mode\n\n",
           llm_router_mode_to_string(llm_router_get_mode()));

    /* Initialize narrative enhancer */
    if (narrative_enhancer_init() != 0) {
        fprintf(stderr, "Narrative enhancer init failed\n");
        return 1;
    }

    printf("✅ Narrative enhancer initialized\n\n");

    /* Test 1: Arrival postcard */
    printf("Test 1: Creating arrival postcard...\n");
    if (narrative_enhancer_create_postcard(
            "West of House",
            "arrival",
            "You are standing in an open field west of a white house with a boarded front door. There is a small mailbox here.") == 0) {
        printf("✅ Arrival postcard created\n\n");
    } else {
        printf("❌ Failed to create arrival postcard\n\n");
    }

    /* Test 2: Death postcard */
    printf("Test 2: Creating death postcard...\n");
    if (narrative_enhancer_create_postcard(
            "Darkness",
            "death",
            "You have been eaten by a grue.") == 0) {
        printf("✅ Death postcard created\n\n");
    } else {
        printf("❌ Failed to create death postcard\n\n");
    }

    /* Display postcards */
    printf("=== Displaying Postcards ===\n");
    narrative_enhancer_display_postcards();

    /* Cleanup */
    narrative_enhancer_shutdown();
    llm_router_shutdown();

    return 0;
}
