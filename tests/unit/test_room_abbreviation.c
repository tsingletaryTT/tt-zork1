/**
 * test_room_abbreviation.c - Unit tests for room name abbreviation
 *
 * Tests the abbreviation logic without Z-machine dependencies.
 * Tests room_abbreviate() function in isolation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../../src/journey/room_names.h"

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

/*
 * Test Cases
 */

void test_abbreviate_west_of_house(void) {
    TEST("'West of House' → 'W.House'");

    char result[64];
    room_abbreviate("West of House", result, sizeof(result));

    if (strcmp(result, "W.House") == 0) {
        PASS();
    } else {
        FAIL("Expected 'W.House'");
    }
}

void test_abbreviate_north_of_house(void) {
    TEST("'North of House' → 'N.House'");

    char result[64];
    room_abbreviate("North of House", result, sizeof(result));

    if (strcmp(result, "N.House") == 0) {
        PASS();
    } else {
        FAIL("Expected 'N.House'");
    }
}

void test_abbreviate_south_of_house(void) {
    TEST("'South of House' → 'S.House'");

    char result[64];
    room_abbreviate("South of House", result, sizeof(result));

    if (strcmp(result, "S.House") == 0) {
        PASS();
    } else {
        FAIL("Expected 'S.House'");
    }
}

void test_abbreviate_east_of_house(void) {
    TEST("'East of House' → 'E.House'");

    char result[64];
    room_abbreviate("East of House", result, sizeof(result));

    if (strcmp(result, "E.House") == 0) {
        PASS();
    } else {
        FAIL("Expected 'E.House'");
    }
}

void test_abbreviate_behind_house(void) {
    TEST("'Behind House' → 'Behind House'");

    char result[64];
    room_abbreviate("Behind House", result, sizeof(result));

    if (strcmp(result, "Behind House") == 0) {
        PASS();
    } else {
        FAIL("Expected 'Behind House'");
    }
}

void test_removes_of_word(void) {
    TEST("Removes 'of' from names");

    char result[64];
    room_abbreviate("Path of Destiny", result, sizeof(result));

    /* Should be "Path Destiny" */
    if (strstr(result, "of") == NULL) {
        PASS();
    } else {
        FAIL("'of' not removed");
    }
}

void test_removes_the_word(void) {
    TEST("Removes 'the' from names");

    char result[64];
    room_abbreviate("The Dark Forest", result, sizeof(result));

    /* Should be "Dark Forest" */
    if (strstr(result, "the") == NULL && strstr(result, "The") == NULL) {
        PASS();
    } else {
        FAIL("'the' not removed");
    }
}

void test_removes_and_word(void) {
    TEST("Removes 'and' from names");

    char result[64];
    room_abbreviate("Dark and Winding Passage", result, sizeof(result));

    /* Should be "Dark Winding Passage" */
    if (strstr(result, "and") == NULL) {
        PASS();
    } else {
        FAIL("'and' not removed");
    }
}

void test_abbreviate_northeast(void) {
    TEST("'Northeast Corner' → 'NE Corner'");

    char result[64];
    room_abbreviate("Northeast Corner", result, sizeof(result));

    if (strstr(result, "NE") != NULL) {
        PASS();
    } else {
        FAIL("Expected 'NE' abbreviation");
    }
}

void test_truncates_long_names(void) {
    TEST("Truncates names longer than 12 chars");

    char result[64];
    room_abbreviate("Very Long Room Name With Many Words", result, sizeof(result));

    if (strlen(result) <= 12) {
        PASS();
    } else {
        FAIL("Name not truncated");
    }
}

void test_handles_empty_string(void) {
    TEST("Handles empty string gracefully");

    char result[64];
    int ret = room_abbreviate("", result, sizeof(result));

    if (ret == 0 && strlen(result) > 0) {
        PASS();
    } else {
        FAIL("Empty string not handled");
    }
}

void test_handles_null_input(void) {
    TEST("Returns error for NULL input");

    char result[64];
    int ret = room_abbreviate(NULL, result, sizeof(result));

    if (ret == -1) {
        PASS();
    } else {
        FAIL("Should return error for NULL");
    }
}

void test_handles_single_word(void) {
    TEST("'Forest' → 'Forest' (unchanged)");

    char result[64];
    room_abbreviate("Forest", result, sizeof(result));

    if (strcmp(result, "Forest") == 0) {
        PASS();
    } else {
        FAIL("Single word changed unexpectedly");
    }
}

void test_preserves_capitalization(void) {
    TEST("Preserves original capitalization");

    char result[64];
    room_abbreviate("Behind House", result, sizeof(result));

    /* Should keep 'B' and 'H' capitalized */
    if (result[0] == 'B' && strstr(result, "House")[0] == 'H') {
        PASS();
    } else {
        FAIL("Capitalization not preserved");
    }
}

void test_no_space_after_period(void) {
    TEST("No space after period in 'W.House'");

    char result[64];
    room_abbreviate("West of House", result, sizeof(result));

    /* Should be "W.House" not "W. House" */
    if (strstr(result, ". ") == NULL) {
        PASS();
    } else {
        FAIL("Unwanted space after period");
    }
}

/*
 * Test Runner
 */

int main(void) {
    printf("\n=== Room Name Abbreviation Unit Tests ===\n\n");

    /* Run all tests */
    test_abbreviate_west_of_house();
    test_abbreviate_north_of_house();
    test_abbreviate_south_of_house();
    test_abbreviate_east_of_house();
    test_abbreviate_behind_house();
    test_removes_of_word();
    test_removes_the_word();
    test_removes_and_word();
    test_abbreviate_northeast();
    test_truncates_long_names();
    test_handles_empty_string();
    test_handles_null_input();
    test_handles_single_word();
    test_preserves_capitalization();
    test_no_space_after_period();

    /* Summary */
    printf("\n=== Test Summary ===\n");
    printf("Total:  %d\n", tests_run);
    printf("Passed: \033[32m%d\033[0m\n", tests_passed);
    printf("Failed: \033[31m%d\033[0m\n", tests_failed);
    printf("====================\n\n");

    return (tests_failed == 0) ? 0 : 1;
}
