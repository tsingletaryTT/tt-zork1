/**
 * test-map-visual.c - Visual demonstration of the journey map
 *
 * This program creates a sample journey and displays the enhanced 2D map.
 * Compile and run:
 *   cc -I. test-map-visual.c src/journey/map_generator.c src/journey/tracker.c -o test-map-visual && ./test-map-visual
 */

#include <stdio.h>
#include <string.h>
#include "src/journey/map_generator.h"
#include "src/journey/tracker.h"

int main(void) {
    printf("\n=== Journey Map Visual Test ===\n\n");
    printf("Creating a sample journey through Zork...\n\n");

    /* Initialize journey tracker */
    journey_init(50);

    /* Simulate a journey:
     * Start at West of House
     * Go North to North of House
     * Go East to Forest
     * Go South to South of House
     * Go West back to West of House (completing a loop)
     * Go North again
     * Go East to Behind House
     */
    journey_record_move(64, "W.House", DIR_UNKNOWN);     /* Starting position */
    journey_record_move(137, "N.House", DIR_NORTH);      /* North */
    journey_record_move(76, "Forest", DIR_EAST);         /* East */
    journey_record_move(209, "S.House", DIR_SOUTH);      /* South */
    journey_record_move(64, "W.House", DIR_WEST);        /* West (back to start) */
    journey_record_move(137, "N.House", DIR_NORTH);      /* North again */
    journey_record_move(85, "Behind", DIR_EAST);         /* East to behind house */

    /* Get the journey history */
    journey_history_t *history = journey_get_history();

    printf("Journey recorded %zu steps:\n", history->count);
    for (size_t i = 0; i < history->count; i++) {
        printf("  %zu. %s\n", i+1, history->steps[i].room_name);
    }
    printf("\n");

    /* Generate the 2D map */
    char map_buffer[8192];
    int result = map_generate(history, map_buffer, sizeof(map_buffer));

    if (result == 0) {
        printf("Generated 2D spatial map:\n");
        printf("%s\n", map_buffer);
    } else {
        printf("Error generating map: code %d\n", result);
    }

    /* Cleanup */
    journey_shutdown();

    printf("\n=== Test Complete ===\n\n");
    printf("The map above shows rooms positioned in 2D space based on\n");
    printf("the directions traveled. Arrows (^v<>) indicate connections.\n\n");

    return 0;
}
