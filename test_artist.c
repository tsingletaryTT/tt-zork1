#include <stdio.h>
#include "src/llm/llm_router.h"
#include "src/llm/scene_visualizer.h"

int main() {
    printf("=== Testing Artist Agent (Chip 1) ===\n\n");

    /* Initialize router */
    if (llm_router_init("config/llm_mode.yaml") != 0) {
        fprintf(stderr, "Router init failed: %s\n", llm_router_get_last_error());
        return 1;
    }

    printf("✅ Router initialized in %s mode\n\n",
           llm_router_mode_to_string(llm_router_get_mode()));

    /* Initialize visualizer */
    if (scene_visualizer_init() != 0) {
        fprintf(stderr, "Visualizer init failed\n");
        return 1;
    }

    printf("✅ Scene visualizer initialized\n\n");

    /* Test with West of House */
    const char *location = "West of House";
    const char *description = "You are standing in an open field west of a white house, with a boarded front door. There is a small mailbox here.";

    printf("Generating ASCII art for: %s\n", location);
    printf("Description: %s\n\n", description);

    if (scene_visualizer_generate(location, description) == 0) {
        printf("\n✅ Art generated successfully!\n");
    } else {
        printf("\n❌ Art generation failed: %s\n", llm_router_get_last_error());
    }

    scene_visualizer_shutdown();
    llm_router_shutdown();

    return 0;
}
