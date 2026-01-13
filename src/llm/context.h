/**
 * context.h - Game Context Manager for LLM
 *
 * EDUCATIONAL PROJECT - READ THIS FIRST!
 *
 * ## What This Module Does
 *
 * This module captures and stores the conversation history between the player
 * and the game. It's like recording everything that happens so the LLM can
 * understand what's going on and make better translations.
 *
 * Think of it as the LLM's "memory" of the game session.
 *
 * ## Why We Need This
 *
 * Without context, the LLM only sees:
 *   User: "open it"
 *   LLM: "open what??"
 *
 * With context, the LLM sees:
 *   Game: "There is a small mailbox here."
 *   User: "open it"
 *   LLM: "open mailbox" ✓
 *
 * ## Architecture Context
 *
 * ```
 * Game outputs text → output_capture.c → context_add_output()
 *                                            ↓
 *                                      [Circular Buffer]
 *                                      stores last N turns
 *                                            ↓
 * User types input → input_translator.c → context_get_formatted()
 *                                            ↓
 *                                   Returns: "Turn 1: ..."
 *                                           "Turn 2: ..."
 *                                           ...
 *                                            ↓
 *                                    Sent to LLM with prompt
 * ```
 *
 * ## Key Concepts
 *
 * **Turn**: One complete interaction cycle
 *   - Game output: "West of House. There is a mailbox here."
 *   - User input: "open mailbox"
 *   - Translated: "open mailbox"
 *
 * **Circular Buffer**: Fixed-size buffer that wraps around
 *   - Stores last N turns (e.g., 20)
 *   - Old turns automatically discarded when buffer full
 *   - Memory-efficient: fixed size regardless of game length
 *
 * ## Future: LLM-to-LLM Gameplay
 *
 * This context manager enables autonomous LLM agents to play the game:
 * - Agent sees full game state via context_get_formatted()
 * - Makes decisions based on history
 * - We can record entire playthroughs for training data
 *
 * ## Key Functions
 *
 * - `context_init()` - Initialize context storage
 * - `context_add_output()` - Append game output to current turn
 * - `context_add_input()` - Record user input and start new turn
 * - `context_get_formatted()` - Get formatted history for LLM
 * - `context_clear()` - Reset history
 * - `context_shutdown()` - Free resources
 */

#ifndef TT_ZORK_CONTEXT_H
#define TT_ZORK_CONTEXT_H

#include <stddef.h>

/**
 * Initialize the context manager
 *
 * Sets up circular buffer to store conversation history.
 * Call this once at startup before any game output.
 *
 * @param max_turns Maximum number of turns to remember (e.g., 20)
 * @return 0 on success, -1 on error
 */
int context_init(size_t max_turns);

/**
 * Add game output to the current turn
 *
 * Game output is accumulated until the next user input.
 * Multiple outputs in a single turn are concatenated.
 *
 * Example:
 *   context_add_output("West of House\n");
 *   context_add_output("There is a small mailbox here.\n");
 *   // Both stored in same turn
 *
 * @param output Game output text (can be partial line)
 */
void context_add_output(const char *output);

/**
 * Add user input and complete the current turn
 *
 * Records the user's input and the LLM's translation, then
 * starts a new turn for future outputs.
 *
 * @param user_text User's natural language input
 * @param translated_commands Commands the LLM produced
 */
void context_add_input(const char *user_text, const char *translated_commands);

/**
 * Get formatted context for LLM prompt
 *
 * Returns a formatted string containing recent game history.
 * Format:
 *   Turn 1 Output: West of House...
 *   Turn 1 Input: go north (translated: north)
 *   Turn 2 Output: North of House...
 *   ...
 *
 * @param buffer Output buffer to write formatted context
 * @param buffer_size Size of output buffer
 * @return 0 on success, -1 if buffer too small
 */
int context_get_formatted(char *buffer, size_t buffer_size);

/**
 * Clear all context history
 *
 * Resets to empty state as if game just started.
 * Useful for debugging or starting new game session.
 */
void context_clear(void);

/**
 * Get current turn number
 *
 * Useful for debugging and logging.
 *
 * @return Current turn count (0 = no turns yet)
 */
size_t context_get_turn_count(void);

/**
 * Shut down context manager and free resources
 */
void context_shutdown(void);

#endif /* TT_ZORK_CONTEXT_H */
