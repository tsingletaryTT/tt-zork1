/**
 * test_game_state.c - Unit tests for game state detection
 *
 * Tests death/victory/quit detection without Z-machine dependencies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../../src/journey/game_state.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    printf("  %-50s", name); \
    fflush(stdout); \
    tests_run++;

#define PASS() \
    printf("[\033[32mPASS\033[0m]\n"); \
    tests_passed++;

#define FAIL(msg) \
    printf("[\033[31mFAIL\033[0m] %s\n", msg); \
    tests_failed++;

static void suppress_stderr(void) {
    freopen("/dev/null", "w", stderr);
}

static void restore_stderr(void) {
    freopen("/dev/tty", "w", stderr);
}

/*
 * Test Cases
 */

void test_init_success(void) {
    TEST("game_state_init() succeeds");

    suppress_stderr();
    int result = game_state_init();
    game_state_shutdown();
    restore_stderr();

    if (result == 0) {
        PASS();
    } else {
        FAIL("Expected init to return 0");
    }
}

void test_initial_reason_unknown(void) {
    TEST("Initial game end reason is UNKNOWN");

    suppress_stderr();
    game_state_init();
    game_end_reason_t reason = game_state_get_reason();
    game_state_shutdown();
    restore_stderr();

    if (reason == GAME_END_UNKNOWN) {
        PASS();
    } else {
        FAIL("Expected GAME_END_UNKNOWN initially");
    }
}

void test_detect_death_pattern_asterisks(void) {
    TEST("Detects '****  You have died  ****'");

    suppress_stderr();
    game_state_init();
    game_state_watch_output("****  You have died  ****");
    game_end_reason_t reason = game_state_get_reason();
    game_state_shutdown();
    restore_stderr();

    if (reason == GAME_END_DEATH) {
        PASS();
    } else {
        FAIL("Expected GAME_END_DEATH");
    }
}

void test_detect_death_pattern_simple(void) {
    TEST("Detects 'You have died'");

    suppress_stderr();
    game_state_init();
    game_state_watch_output("You have died.");
    game_end_reason_t reason = game_state_get_reason();
    game_state_shutdown();
    restore_stderr();

    if (reason == GAME_END_DEATH) {
        PASS();
    } else {
        FAIL("Expected GAME_END_DEATH");
    }
}

void test_detect_death_pattern_case_insensitive(void) {
    TEST("Death detection is case-insensitive");

    suppress_stderr();
    game_state_init();
    game_state_watch_output("YOU HAVE DIED");
    game_end_reason_t reason = game_state_get_reason();
    game_state_shutdown();
    restore_stderr();

    if (reason == GAME_END_DEATH) {
        PASS();
    } else {
        FAIL("Expected case-insensitive detection");
    }
}

void test_detect_death_you_are_dead(void) {
    TEST("Detects 'You are dead'");

    suppress_stderr();
    game_state_init();
    game_state_watch_output("You are dead.");
    game_end_reason_t reason = game_state_get_reason();
    game_state_shutdown();
    restore_stderr();

    if (reason == GAME_END_DEATH) {
        PASS();
    } else {
        FAIL("Expected GAME_END_DEATH");
    }
}

void test_detect_victory_pattern_asterisks(void) {
    TEST("Detects '****  You have won  ****'");

    suppress_stderr();
    game_state_init();
    game_state_watch_output("****  You have won  ****");
    game_end_reason_t reason = game_state_get_reason();
    game_state_shutdown();
    restore_stderr();

    if (reason == GAME_END_VICTORY) {
        PASS();
    } else {
        FAIL("Expected GAME_END_VICTORY");
    }
}

void test_detect_victory_congratulations(void) {
    TEST("Detects 'Congratulations!'");

    suppress_stderr();
    game_state_init();
    game_state_watch_output("Congratulations! You won!");
    game_end_reason_t reason = game_state_get_reason();
    game_state_shutdown();
    restore_stderr();

    if (reason == GAME_END_VICTORY) {
        PASS();
    } else {
        FAIL("Expected GAME_END_VICTORY");
    }
}

void test_no_false_positive_normal_text(void) {
    TEST("No false positive on normal text");

    suppress_stderr();
    game_state_init();
    game_state_watch_output("You are in a forest.");
    game_state_watch_output("You see a house.");
    game_end_reason_t reason = game_state_get_reason();
    game_state_shutdown();
    restore_stderr();

    if (reason == GAME_END_UNKNOWN) {
        PASS();
    } else {
        FAIL("False positive detected");
    }
}

void test_user_quit_sets_reason(void) {
    TEST("game_state_set_user_quit() sets reason");

    suppress_stderr();
    game_state_init();
    game_state_set_user_quit();
    game_end_reason_t reason = game_state_get_reason();
    game_state_shutdown();
    restore_stderr();

    if (reason == GAME_END_USER_QUIT) {
        PASS();
    } else {
        FAIL("Expected GAME_END_USER_QUIT");
    }
}

void test_should_show_map_for_death(void) {
    TEST("should_show_map() = TRUE for death");

    suppress_stderr();
    game_state_init();
    game_state_watch_output("You have died");
    int should_show = game_state_should_show_map();
    game_state_shutdown();
    restore_stderr();

    if (should_show == 1) {
        PASS();
    } else {
        FAIL("Expected map to show for death");
    }
}

void test_should_show_map_for_victory(void) {
    TEST("should_show_map() = TRUE for victory");

    suppress_stderr();
    game_state_init();
    game_state_watch_output("You have won");
    int should_show = game_state_should_show_map();
    game_state_shutdown();
    restore_stderr();

    if (should_show == 1) {
        PASS();
    } else {
        FAIL("Expected map to show for victory");
    }
}

void test_should_not_show_map_for_user_quit(void) {
    TEST("should_show_map() = FALSE for user quit");

    suppress_stderr();
    game_state_init();
    game_state_set_user_quit();
    int should_show = game_state_should_show_map();
    game_state_shutdown();
    restore_stderr();

    if (should_show == 0) {
        PASS();
    } else {
        FAIL("Expected map NOT to show for user quit");
    }
}

void test_should_not_show_map_for_unknown(void) {
    TEST("should_show_map() = FALSE for unknown reason");

    suppress_stderr();
    game_state_init();
    int should_show = game_state_should_show_map();
    game_state_shutdown();
    restore_stderr();

    if (should_show == 0) {
        PASS();
    } else {
        FAIL("Expected map NOT to show for unknown");
    }
}

void test_reset_clears_state(void) {
    TEST("game_state_reset() clears state");

    suppress_stderr();
    game_state_init();
    game_state_watch_output("You have died");
    game_state_reset();
    game_end_reason_t reason = game_state_get_reason();
    game_state_shutdown();
    restore_stderr();

    if (reason == GAME_END_UNKNOWN) {
        PASS();
    } else {
        FAIL("Expected reset to clear state");
    }
}

void test_reason_string_descriptions(void) {
    TEST("game_state_get_reason_string() returns descriptions");

    suppress_stderr();
    game_state_init();

    game_state_watch_output("You have died");
    const char *death_str = game_state_get_reason_string();

    game_state_reset();
    game_state_watch_output("You have won");
    const char *victory_str = game_state_get_reason_string();

    game_state_reset();
    game_state_set_user_quit();
    const char *quit_str = game_state_get_reason_string();

    game_state_shutdown();
    restore_stderr();

    int valid = (strcmp(death_str, "Death") == 0 &&
                 strcmp(victory_str, "Victory") == 0 &&
                 strcmp(quit_str, "User Quit") == 0);

    if (valid) {
        PASS();
    } else {
        FAIL("Reason strings incorrect");
    }
}

void test_empty_text_ignored(void) {
    TEST("Empty/whitespace text is ignored");

    suppress_stderr();
    game_state_init();
    game_state_watch_output("");
    game_state_watch_output("   ");
    game_state_watch_output("\n");
    game_end_reason_t reason = game_state_get_reason();
    game_state_shutdown();
    restore_stderr();

    if (reason == GAME_END_UNKNOWN) {
        PASS();
    } else {
        FAIL("Empty text caused state change");
    }
}

/*
 * Test Runner
 */

int main(void) {
    printf("\n=== Game State Detection Unit Tests ===\n\n");

    /* Run all tests */
    test_init_success();
    test_initial_reason_unknown();
    test_detect_death_pattern_asterisks();
    test_detect_death_pattern_simple();
    test_detect_death_pattern_case_insensitive();
    test_detect_death_you_are_dead();
    test_detect_victory_pattern_asterisks();
    test_detect_victory_congratulations();
    test_no_false_positive_normal_text();
    test_user_quit_sets_reason();
    test_should_show_map_for_death();
    test_should_show_map_for_victory();
    test_should_not_show_map_for_user_quit();
    test_should_not_show_map_for_unknown();
    test_reset_clears_state();
    test_reason_string_descriptions();
    test_empty_text_ignored();

    /* Summary */
    printf("\n=== Test Summary ===\n");
    printf("Total:  %d\n", tests_run);
    printf("Passed: \033[32m%d\033[0m\n", tests_passed);
    printf("Failed: \033[31m%d\033[0m\n", tests_failed);
    printf("====================\n\n");

    return (tests_failed == 0) ? 0 : 1;
}
