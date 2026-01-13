/**
 * input_translator.h - Natural Language to Command Translation
 *
 * EDUCATIONAL PROJECT - The Heart of the System!
 *
 * ## What This Module Does
 *
 * This is the main orchestrator that brings all the pieces together to
 * translate natural language into Zork commands. It's like a conductor
 * leading an orchestra - each instrument (module) plays its part.
 *
 * ## The Translation Flow (Orchestra Analogy)
 *
 * ```
 * User: "I want to go through the door"
 *    ↓
 * [1. Context Manager - Strings Section]
 *    "What happened recently in the game?"
 *    ← "West of House. There is a white door."
 *    ↓
 * [2. Prompt Loader - Woodwinds Section]
 *    "How should we format the request?"
 *    ← System: "You are a translator..."
 *    ← User: "{CONTEXT}... Translate: {INPUT}"
 *    ↓
 * [3. LLM Client - Brass Section (the power!)]
 *    "Make the API call!"
 *    → HTTP POST with formatted prompts
 *    ← "north, open door"
 *    ↓
 * [4. Display & Record - Percussion Section]
 *    Show user: "[LLM → north, open door]"
 *    Record in context for next time
 *    ↓
 * Return: "north, open door"
 * ```
 *
 * ## Architecture Context
 *
 * This module sits between Frotz's input system and the Z-machine:
 *
 * ```
 * os_read_line() in dinput.c
 *        ↓
 * Gets user input: "go through door"
 *        ↓
 * translator_process() ← YOU ARE HERE!
 *        ↓
 * Returns: "north, open door"
 *        ↓
 * Frotz sends to Z-machine parser
 * ```
 *
 * ## Error Handling & Fallback
 *
 * If LLM fails (network down, timeout, etc.):
 * 1. Log the error
 * 2. Return original input unchanged
 * 3. Game continues normally
 *
 * **The game must ALWAYS be playable**, even if LLM is broken!
 *
 * ## Future: LLM-to-LLM Gameplay
 *
 * To have an LLM agent play autonomously:
 * 1. Agent reads game output via context_get_formatted()
 * 2. Agent decides action (different LLM call)
 * 3. Action goes through translator_process() (or directly to Z-machine)
 * 4. Repeat
 *
 * This module's design supports that - input source is pluggable.
 *
 * ## Key Functions
 *
 * - `translator_init()` - Initialize all subsystems
 * - `translator_process()` - Main translation function
 * - `translator_is_enabled()` - Check if LLM is active
 * - `translator_shutdown()` - Clean up all resources
 */

#ifndef TT_ZORK_INPUT_TRANSLATOR_H
#define TT_ZORK_INPUT_TRANSLATOR_H

#include <stddef.h>

/**
 * Initialize the input translator
 *
 * This initializes ALL LLM subsystems:
 * - Context manager
 * - Prompt loader
 * - LLM client
 * - Output capture
 *
 * Call once at game startup.
 *
 * @return 0 on success, -1 if LLM unavailable (game still playable)
 */
int translator_init(void);

/**
 * Process user input and translate to commands
 *
 * This is the main function called for each user input.
 *
 * Flow:
 * 1. Check if LLM is enabled
 * 2. Get game context
 * 3. Format prompts
 * 4. Call LLM API
 * 5. Display translation to user
 * 6. Record in context
 * 7. Return translated commands
 *
 * If any step fails, returns original input (fallback).
 *
 * @param user_input User's natural language input
 * @param output_commands Buffer for translated commands
 * @param output_size Size of output buffer
 * @return 0 on success, -1 if using fallback
 */
int translator_process(const char *user_input,
                        char *output_commands,
                        size_t output_size);

/**
 * Check if translator is enabled and working
 *
 * Returns false if:
 * - LLM is disabled via config
 * - Initialization failed
 * - Network unreachable
 *
 * @return 1 if enabled, 0 if disabled/unavailable
 */
int translator_is_enabled(void);

/**
 * Get statistics about translation
 *
 * Useful for debugging and logging.
 *
 * @param total_translations Output: total number of translations attempted
 * @param successful Output: number of successful LLM calls
 * @param fallbacks Output: number of times we fell back to original input
 */
void translator_get_stats(int *total_translations,
                           int *successful,
                           int *fallbacks);

/**
 * Shut down translator and all subsystems
 */
void translator_shutdown(void);

#endif /* TT_ZORK_INPUT_TRANSLATOR_H */
