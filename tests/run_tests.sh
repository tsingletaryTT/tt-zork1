#!/usr/bin/env bash
#
# run_tests.sh - Test runner for journey tracking features
#
# Usage:
#   ./tests/run_tests.sh           # Run all tests
#   ./tests/run_tests.sh unit      # Run only unit tests
#   ./tests/run_tests.sh integration # Run only integration tests
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

# Build configuration
CC="${CC:-cc}"
CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter -I. -g"

# Test mode
TEST_MODE="${1:-all}"

echo -e "${BLUE}=== Journey Tracking Test Suite ===${NC}\n"

# Function to compile a test
compile_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .c)
    local output="tests/unit/$test_name"
    local sources=""

    echo -e "${YELLOW}Compiling${NC} $test_name..."

    # Determine which source files to include based on test
    if [[ "$test_name" == "test_tracker" ]]; then
        sources="src/journey/tracker.c"
    elif [[ "$test_name" == "test_game_state" ]]; then
        sources="src/journey/game_state.c"
    elif [[ "$test_name" == "test_map_generator" ]]; then
        sources="src/journey/map_generator.c src/journey/tracker.c"
    elif [[ "$test_name" == "test_room_abbreviation" ]]; then
        # Only compile room_names.c if we need Z-machine mocks
        # For abbreviation tests, we don't need the full decoder
        sources="src/journey/room_names.c"
        # Add stubs for Z-machine symbols to avoid linking full Z-machine
        cat > /tmp/zmachine_stub.c << 'EOF'
/* Stubs for Z-machine symbols when testing in isolation */
#include <stddef.h>

typedef unsigned short zword;
typedef unsigned char zbyte;

#define LOW_WORD(addr,var) var=0
#define LOW_BYTE(addr,var) var=0

/* Stub Z-machine memory pointer (used by decoder) */
zbyte *zmp = NULL;

/* Stub for object_name function */
zword object_name(zword object) {
    return 0;  /* Stub: return 0 address */
}
EOF
        sources="src/journey/room_names.c /tmp/zmachine_stub.c"
    fi

    # Compile
    if $CC $CFLAGS "$test_file" $sources -o "$output" 2>/dev/null; then
        echo -e "  ${GREEN}✓${NC} Compiled successfully"
        return 0
    else
        echo -e "  ${RED}✗${NC} Compilation failed"
        return 1
    fi
}

# Function to run a test
run_test() {
    local test_binary="$1"
    local test_name=$(basename "$test_binary")

    if [ ! -x "$test_binary" ]; then
        echo -e "${RED}Error:${NC} $test_name not found or not executable"
        return 1
    fi

    # Run the test and capture result
    if "$test_binary"; then
        return 0
    else
        return 1
    fi
}

# Compile and run unit tests
run_unit_tests() {
    echo -e "${BLUE}=== Unit Tests ===${NC}\n"

    local total=0
    local passed=0
    local failed=0

    for test_file in tests/unit/test_*.c; do
        if [ ! -f "$test_file" ]; then
            continue
        fi

        test_name=$(basename "$test_file" .c)
        test_binary="tests/unit/$test_name"

        # Compile
        if ! compile_test "$test_file"; then
            ((failed++))
            ((total++))
            continue
        fi

        # Run
        if run_test "$test_binary"; then
            ((passed++))
        else
            ((failed++))
        fi
        ((total++))

        echo ""
    done

    echo -e "${BLUE}=== Unit Test Summary ===${NC}"
    echo -e "Total test suites: $total"
    echo -e "Passed: ${GREEN}$passed${NC}"
    echo -e "Failed: ${RED}$failed${NC}"
    echo ""

    return $failed
}

# Run integration tests
run_integration_tests() {
    echo -e "${BLUE}=== Integration Tests ===${NC}\n"

    # Build the main binary first
    echo "Building zork-native..."
    if ! ./scripts/build_local.sh debug >/dev/null 2>&1; then
        echo -e "${RED}Failed to build zork-native${NC}"
        return 1
    fi
    echo -e "${GREEN}✓${NC} Built zork-native\n"

    # Run integration test scenarios
    echo "Running integration tests..."

    local passed=0
    local failed=0

    # Test 1: User quit should not show map
    echo -n "  Test: User quit (no map)... "
    if printf "look\nquit\ny\n" | ./zork-native game/zork1.z3 2>&1 | grep -q "Game state: USER QUIT"; then
        if ! printf "look\nquit\ny\n" | ./zork-native game/zork1.z3 2>&1 | grep -q "JOURNEY MAP PLACEHOLDER"; then
            echo -e "${GREEN}PASS${NC}"
            ((passed++))
        else
            echo -e "${RED}FAIL${NC} (map was shown)"
            ((failed++))
        fi
    else
        echo -e "${RED}FAIL${NC} (quit not detected)"
        ((failed++))
    fi

    # Test 2: Journey tracking records moves
    echo -n "  Test: Journey records moves... "
    move_count=$(printf "north\neast\nsouth\nquit\ny\n" | ./zork-native game/zork1.z3 2>&1 | grep "^Journey:" | wc -l | tr -d ' ')
    if [ "$move_count" -ge 3 ]; then
        echo -e "${GREEN}PASS${NC} ($move_count moves)"
        ((passed++))
    else
        echo -e "${RED}FAIL${NC} (only $move_count moves)"
        ((failed++))
    fi

    # Test 3: Room names are abbreviated
    echo -n "  Test: Room name abbreviation... "
    if printf "quit\ny\n" | ./zork-native game/zork1.z3 2>&1 | grep -q "W.House"; then
        echo -e "${GREEN}PASS${NC}"
        ((passed++))
    else
        echo -e "${RED}FAIL${NC} (abbreviation not working)"
        ((failed++))
    fi

    # Test 4: Directions are tracked
    echo -n "  Test: Direction tracking... "
    if printf "north\nquit\ny\n" | ./zork-native game/zork1.z3 2>&1 | grep -q "via N"; then
        echo -e "${GREEN}PASS${NC}"
        ((passed++))
    else
        echo -e "${RED}FAIL${NC} (direction not tracked)"
        ((failed++))
    fi

    echo ""
    echo -e "${BLUE}=== Integration Test Summary ===${NC}"
    local total=$((passed + failed))
    echo -e "Total tests: $total"
    echo -e "Passed: ${GREEN}$passed${NC}"
    echo -e "Failed: ${RED}$failed${NC}"
    echo ""

    return $failed
}

# Main execution
main() {
    local exit_code=0

    case "$TEST_MODE" in
        unit)
            run_unit_tests || exit_code=$?
            ;;
        integration)
            run_integration_tests || exit_code=$?
            ;;
        all|*)
            run_unit_tests || exit_code=$?
            echo ""
            run_integration_tests || exit_code=$((exit_code + $?))
            ;;
    esac

    echo ""
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✓ All tests passed!${NC}"
    else
        echo -e "${RED}✗ Some tests failed${NC}"
    fi

    return $exit_code
}

# Run main
main
exit $?
