# Journey Tracking Tests

This directory contains unit and integration tests for the journey tracking features.

## Test Structure

```
tests/
├── unit/           - Unit tests for individual modules
│   ├── test_tracker.c       - Journey history tracking
│   ├── test_room_names.c    - Room name extraction/abbreviation
│   ├── test_game_state.c    - Game end detection
│   └── test_monitor.c       - Location change monitoring
├── integration/    - End-to-end integration tests
│   └── test_journey_e2e.sh  - Full gameplay scenarios
├── mocks/          - Mock Z-machine functions for testing
│   └── zmachine_mocks.c     - Fake Frotz functions
└── run_tests.sh    - Test runner script
```

## Running Tests

```bash
# Run all tests
./tests/run_tests.sh

# Run only unit tests
./tests/run_tests.sh unit

# Run only integration tests
./tests/run_tests.sh integration

# Run specific test
./tests/unit/test_tracker
```

## Test Philosophy

**Unit Tests**: Test individual modules in isolation using mocks
- Fast execution (< 1ms per test)
- No dependencies on Z-machine or game files
- Focus on logic and edge cases

**Integration Tests**: Test full system with real game
- Use actual Zork game file
- Verify end-to-end behavior
- Slower but comprehensive

## Coverage Goals

- Tracker module: 100% coverage
- Room names: 100% coverage
- Game state: 100% coverage
- Monitor: 90% coverage (some Z-machine dependencies)

## Writing New Tests

Use the existing test files as templates. Each test should:
1. Have a clear name describing what it tests
2. Set up test data/state
3. Execute the code under test
4. Assert expected results
5. Clean up (if needed)

Example:
```c
void test_tracker_records_move(void) {
    journey_init(10);

    int result = journey_record_move(64, "W.House", DIR_NORTH);

    assert(result == 0);
    assert(journey_get_step_count() == 1);

    journey_shutdown();
}
```
