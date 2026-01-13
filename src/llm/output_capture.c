/**
 * output_capture.c - Output Capture Implementation
 *
 * EDUCATIONAL PROJECT - Simple but powerful!
 *
 * This is one of the simplest modules in our system, yet it's critical.
 * It's a great example of the "wrapper pattern" - adding functionality
 * by wrapping existing functions.
 *
 * ## Implementation Strategy
 *
 * We could implement output capture in several ways:
 *
 * **Option 1: Direct Hook (What We Do)**
 * - Modify Frotz to call output_capture_add()
 * - Pro: Simple, direct, easy to understand
 * - Con: Requires modifying Frotz source
 *
 * **Option 2: Function Pointer Replacement**
 * - Replace os_display_string pointer at runtime
 * - Pro: No source modification
 * - Con: Complex, fragile, hard to debug
 *
 * **Option 3: LD_PRELOAD (Linux)**
 * - Override functions at load time
 * - Pro: Zero code changes
 * - Con: Platform-specific, confusing for students
 *
 * We chose Option 1 for clarity and educational value.
 *
 * ## Performance Considerations
 *
 * Output capture adds minimal overhead:
 * - Function call: ~10 nanoseconds
 * - String concatenation: depends on length, but text output is infrequent
 * - Total impact: <1% of runtime
 *
 * Game output happens at human speed (reading rate), so performance
 * is completely irrelevant here. Clarity and correctness matter more!
 *
 * ## Thread Safety
 *
 * Not thread-safe, but that's fine - the game is single-threaded.
 * All output happens on the main thread sequentially.
 */

#include "output_capture.h"
#include "context.h"
#include <stdio.h>

/* State tracking */
static struct {
    int initialized;
    int enabled;
} state = {0};

int output_capture_init(void) {
    /* Just mark as initialized */
    state.initialized = 1;
    state.enabled = 1;

    fprintf(stderr, "Output capture: Initialized\n");
    return 0;
}

void output_capture_add(const char *text) {
    /* Guard: Check if initialized and enabled */
    if (!state.initialized || !state.enabled || !text) {
        return;
    }

    /*
     * Forward to context manager
     *
     * This is the core of output capture - we simply pass the text
     * to the context manager which accumulates it for the LLM.
     *
     * The context manager handles:
     * - Accumulating text into turns
     * - Managing circular buffer
     * - Formatting for LLM prompts
     *
     * We just forward the text. Simple!
     */
    context_add_output(text);
}

void output_capture_shutdown(void) {
    state.initialized = 0;
    state.enabled = 0;
}
