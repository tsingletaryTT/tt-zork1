#include <stdio.h>
#include "src/llm/slash_commands.h"

int main() {
    printf("=== Testing Slash Commands ===\n\n");

    /* Initialize */
    slash_commands_init(MODE_ENHANCED, PLAY_SOLO);

    /* Test commands */
    const char *test_commands[] = {
        "/help",
        "/status",
        "/mode classic",
        "/status",
        "/mode enhanced",
        "/play auto",
        "/status",
        "/play solo",
        "/unknown",
        "north",  /* Not a slash command */
        NULL
    };

    for (int i = 0; test_commands[i] != NULL; i++) {
        printf("\n--- Input: \"%s\" ---\n", test_commands[i]);
        slash_result_t result = slash_commands_process(test_commands[i]);

        if (result.handled) {
            printf("✓ Handled: %s\n", result.should_skip ? "skip turn" : "continue");
            if (result.message[0]) {
                printf("%s", result.message);
            }
        } else {
            printf("Not a slash command (send to game)\n");
        }

        /* Show current state */
        printf("Current mode: %s / %s\n",
               slash_commands_get_game_mode() == MODE_CLASSIC ? "classic" : "enhanced",
               slash_commands_get_play_mode() == PLAY_SOLO ? "solo" : "auto");
        printf("Translator: %s, Artist: %s, DM: %s, Auto: %s\n",
               slash_commands_use_translator() ? "ON" : "OFF",
               slash_commands_use_artist() ? "ON" : "OFF",
               slash_commands_use_dm() ? "ON" : "OFF",
               slash_commands_is_auto_play() ? "ON" : "OFF");
    }

    printf("\n=== All tests complete ===\n");
    return 0;
}
