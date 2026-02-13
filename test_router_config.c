#include <stdio.h>
#include "src/llm/llm_router.h"

int main() {
    printf("=== Router Configuration Test ===\n\n");

    /* Initialize router */
    if (llm_router_init("config/llm_mode.yaml") != 0) {
        fprintf(stderr, "Error: %s\n", llm_router_get_last_error());
        return 1;
    }

    printf("✅ Router initialized!\n");
    printf("Mode: %s\n", llm_router_mode_to_string(llm_router_get_mode()));
    printf("Ready: %s\n\n", llm_router_is_ready() ? "YES" : "NO");

    /* Test getting task info */
    char info[512];
    const char *tasks[] = {"TRANSLATE", "VISUALIZE", "NARRATE", "AUTOPLAY"};

    for (int i = 0; i < 4; i++) {
        if (llm_router_get_task_info((LLMTaskType)i, info, sizeof(info)) == 0) {
            printf("Task %s:\n%s\n\n", tasks[i], info);
        } else {
            printf("Task %s: Not configured\n\n", tasks[i]);
        }
    }

    llm_router_shutdown();
    return 0;
}
