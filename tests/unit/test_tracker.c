/**
 * test_tracker.c - Unit tests for journey tracking
 *
 * Tests the core journey history tracking without Z-machine dependencies.
 * This demonstrates isolated unit testing with no external dependencies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Include the module under test */
#include "../../src/journey/tracker.h"

/* Test helpers */
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

/* Suppress stderr output during tests */
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
    TEST("journey_init() succeeds with valid capacity");

    suppress_stderr();
    int result = journey_init(50);
    restore_stderr();

    if (result == 0) {
        journey_shutdown();
        PASS();
    } else {
        FAIL("Expected init to return 0");
    }
}

void test_init_with_zero_capacity(void) {
    TEST("journey_init(0) uses default capacity");

    suppress_stderr();
    int result = journey_init(0);
    restore_stderr();

    if (result == 0) {
        journey_shutdown();
        PASS();
    } else {
        FAIL("Expected init with 0 to succeed with defaults");
    }
}

void test_record_move_basic(void) {
    TEST("journey_record_move() records a single move");

    suppress_stderr();
    journey_init(10);

    int result = journey_record_move(64, "W.House", DIR_NORTH);
    size_t count = journey_get_step_count();

    journey_shutdown();
    restore_stderr();

    if (result == 0 && count == 1) {
        PASS();
    } else {
        FAIL("Expected result=0 and count=1");
    }
}

void test_record_multiple_moves(void) {
    TEST("journey_record_move() records multiple moves");

    suppress_stderr();
    journey_init(10);

    journey_record_move(64, "W.House", DIR_UNKNOWN);
    journey_record_move(137, "N.House", DIR_NORTH);
    journey_record_move(85, "Behind", DIR_EAST);

    size_t count = journey_get_step_count();

    journey_shutdown();
    restore_stderr();

    if (count == 3) {
        PASS();
    } else {
        FAIL("Expected count=3");
    }
}

void test_get_history_returns_valid_pointer(void) {
    TEST("journey_get_history() returns valid pointer");

    suppress_stderr();
    journey_init(10);

    journey_record_move(64, "W.House", DIR_UNKNOWN);

    journey_history_t *history = journey_get_history();

    int valid = (history != NULL && history->count == 1);

    journey_shutdown();
    restore_stderr();

    if (valid) {
        PASS();
    } else {
        FAIL("Expected non-NULL history with count=1");
    }
}

void test_history_contains_correct_data(void) {
    TEST("History contains correct room data");

    suppress_stderr();
    journey_init(10);

    journey_record_move(64, "W.House", DIR_NORTH);

    journey_history_t *history = journey_get_history();

    int valid = (history != NULL &&
                 history->count == 1 &&
                 history->steps[0].room_obj == 64 &&
                 strcmp(history->steps[0].room_name, "W.House") == 0 &&
                 history->steps[0].direction == DIR_NORTH &&
                 history->steps[0].sequence == 0);

    journey_shutdown();
    restore_stderr();

    if (valid) {
        PASS();
    } else {
        FAIL("History data mismatch");
    }
}

void test_array_growth(void) {
    TEST("Dynamic array grows when capacity exceeded");

    suppress_stderr();
    journey_init(2);  /* Start with small capacity */

    /* Add 5 moves (should trigger growth) */
    for (int i = 0; i < 5; i++) {
        journey_record_move(100 + i, "Room", DIR_NORTH);
    }

    size_t count = journey_get_step_count();

    journey_shutdown();
    restore_stderr();

    if (count == 5) {
        PASS();
    } else {
        FAIL("Expected count=5 after growth");
    }
}

void test_clear_resets_history(void) {
    TEST("journey_clear() resets history");

    suppress_stderr();
    journey_init(10);

    journey_record_move(64, "W.House", DIR_NORTH);
    journey_clear();

    size_t count = journey_get_step_count();

    journey_shutdown();
    restore_stderr();

    if (count == 0) {
        PASS();
    } else {
        FAIL("Expected count=0 after clear");
    }
}

void test_last_location_tracking(void) {
    TEST("journey_get_last_location() returns correct room");

    suppress_stderr();
    journey_init(10);

    journey_record_move(64, "W.House", DIR_UNKNOWN);
    journey_record_move(137, "N.House", DIR_NORTH);

    zword last = journey_get_last_location();

    journey_shutdown();
    restore_stderr();

    if (last == 137) {
        PASS();
    } else {
        FAIL("Expected last location = 137");
    }
}

void test_null_room_name_handled(void) {
    TEST("NULL room name is handled gracefully");

    suppress_stderr();
    journey_init(10);

    int result = journey_record_move(64, NULL, DIR_NORTH);

    journey_shutdown();
    restore_stderr();

    if (result == -1) {
        PASS();
    } else {
        FAIL("Expected error for NULL room name");
    }
}

void test_long_room_name_truncated(void) {
    TEST("Long room name is truncated safely");

    suppress_stderr();
    journey_init(10);

    char long_name[100];
    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    journey_record_move(64, long_name, DIR_NORTH);

    journey_history_t *history = journey_get_history();

    /* Should be truncated to fit in room_name[32] */
    int valid = (history != NULL &&
                 strlen(history->steps[0].room_name) < 32);

    journey_shutdown();
    restore_stderr();

    if (valid) {
        PASS();
    } else {
        FAIL("Long name not truncated properly");
    }
}

void test_sequence_numbers_increment(void) {
    TEST("Sequence numbers increment correctly");

    suppress_stderr();
    journey_init(10);

    journey_record_move(64, "Room1", DIR_NORTH);
    journey_record_move(65, "Room2", DIR_SOUTH);
    journey_record_move(66, "Room3", DIR_EAST);

    journey_history_t *history = journey_get_history();

    int valid = (history != NULL &&
                 history->steps[0].sequence == 0 &&
                 history->steps[1].sequence == 1 &&
                 history->steps[2].sequence == 2);

    journey_shutdown();
    restore_stderr();

    if (valid) {
        PASS();
    } else {
        FAIL("Sequence numbers incorrect");
    }
}

void test_revisit_same_room(void) {
    TEST("Revisiting same room is recorded");

    suppress_stderr();
    journey_init(10);

    journey_record_move(64, "W.House", DIR_NORTH);
    journey_record_move(137, "N.House", DIR_EAST);
    journey_record_move(64, "W.House", DIR_WEST);  /* Revisit */

    size_t count = journey_get_step_count();

    journey_shutdown();
    restore_stderr();

    if (count == 3) {
        PASS();
    } else {
        FAIL("Revisit not recorded (expected count=3)");
    }
}

/*
 * Test Runner
 */

int main(void) {
    printf("\n=== Journey Tracker Unit Tests ===\n\n");

    /* Run all tests */
    test_init_success();
    test_init_with_zero_capacity();
    test_record_move_basic();
    test_record_multiple_moves();
    test_get_history_returns_valid_pointer();
    test_history_contains_correct_data();
    test_array_growth();
    test_clear_resets_history();
    test_last_location_tracking();
    test_null_room_name_handled();
    test_long_room_name_truncated();
    test_sequence_numbers_increment();
    test_revisit_same_room();

    /* Summary */
    printf("\n=== Test Summary ===\n");
    printf("Total:  %d\n", tests_run);
    printf("Passed: \033[32m%d\033[0m\n", tests_passed);
    printf("Failed: \033[31m%d\033[0m\n", tests_failed);
    printf("====================\n\n");

    return (tests_failed == 0) ? 0 : 1;
}
