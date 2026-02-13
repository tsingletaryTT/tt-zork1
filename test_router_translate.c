#include <stdio.h>
#include "src/llm/llm_router.h"

int main() {
    printf("=== Testing LLM Translation via Router ===\n\n");

    /* Initialize router */
    if (llm_router_init("config/llm_mode.yaml") != 0) {
        fprintf(stderr, "Error: %s\n", llm_router_get_last_error());
        return 1;
    }

    printf("✅ Router initialized in %s mode\n\n", 
           llm_router_mode_to_string(llm_router_get_mode()));

    /* Test translation */
    const char *input = "I want to open the mailbox";
    char output[512];

    printf("Input: %s\n", input);

    if (llm_router_request(LLM_TASK_TRANSLATE, input, output, sizeof(output)) == 0) {
        printf("✅ Translation successful!\n");
        printf("Output: %s\n", output);
    } else {
        printf("❌ Translation failed!\n");
        printf("Error: %s\n", llm_router_get_last_error());
    }

    llm_router_shutdown();
    return 0;
}
