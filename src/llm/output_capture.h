/**
 * output_capture.h - Game Output Capture for LLM Context
 *
 * EDUCATIONAL PROJECT - READ THIS FIRST!
 *
 * ## What This Module Does
 *
 * This module "spies" on everything the game outputs to the screen and saves
 * it for the LLM to read. Think of it like recording a conversation so you
 * can refer back to it later.
 *
 * ## Why We Need This
 *
 * The LLM needs to see what the game said to make good translations:
 *
 * Game: "There is a brass lantern here."
 * User: "take it"
 * LLM (with context): "take lantern" ✓
 *
 * Without seeing the game output, the LLM wouldn't know what "it" refers to!
 *
 * ## Architecture Context
 *
 * ```
 * Z-machine decides to output text
 *        ↓
 * os_display_string("West of House...")
 *        ↓
 * [Modified to call output_capture_add()]
 *        ↓
 * Still displays to screen (original behavior)
 *   AND
 * Saves to context manager (new behavior)
 * ```
 *
 * This is called the "Observer Pattern" - we observe without changing behavior.
 *
 * ## Integration Points
 *
 * We hook into Frotz at these functions:
 * - `os_display_string()` - Most game text
 * - `os_display_char()` - Single characters
 *
 * Located in: `src/zmachine/frotz/src/dumb/doutput.c`
 *
 * ## Design Philosophy
 *
 * **Non-intrusive**: The game doesn't know we're watching. If LLM is disabled,
 * zero overhead. The game always works normally.
 *
 * **Modular**: Easy to enable/disable. Just don't call our functions.
 *
 * ## Future: LLM-to-LLM Gameplay
 *
 * When an LLM agent plays the game autonomously, it needs to see all output.
 * This module provides that visibility. We could add:
 * - output_capture_get_last_line() for real-time agent decisions
 * - output_capture_export_log() for training data collection
 *
 * ## Key Functions
 *
 * - `output_capture_init()` - Initialize capture system
 * - `output_capture_add()` - Capture game output text
 * - `output_capture_shutdown()` - Clean up
 */

#ifndef TT_ZORK_OUTPUT_CAPTURE_H
#define TT_ZORK_OUTPUT_CAPTURE_H

/**
 * Initialize output capture
 *
 * Sets up connection to context manager.
 * Call once at startup after context_init().
 *
 * @return 0 on success, -1 on error
 */
int output_capture_init(void);

/**
 * Capture game output text
 *
 * Call this whenever the game outputs text to the screen.
 * Text is accumulated and sent to the context manager.
 *
 * Example:
 *   output_capture_add("West of House\n");
 *   output_capture_add("There is a mailbox here.\n");
 *
 * @param text Text to capture (can be partial line or single char)
 */
void output_capture_add(const char *text);

/**
 * Shut down output capture
 *
 * Flushes any pending output and cleans up.
 */
void output_capture_shutdown(void);

#endif /* TT_ZORK_OUTPUT_CAPTURE_H */
