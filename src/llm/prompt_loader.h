/**
 * prompt_loader.h - LLM Prompt File Loader
 *
 * EDUCATIONAL PROJECT - READ THIS FIRST!
 *
 * ## What This Module Does
 *
 * This module loads LLM prompts from separate text files instead of hardcoding
 * them in C strings. This allows easy experimentation with prompt engineering
 * without recompiling the code.
 *
 * ## Architecture Context
 *
 * In the LLM-backed Zork system, prompts are the instructions we give to the
 * language model. They tell it how to translate natural language into Zork
 * commands. By putting prompts in external files, we can:
 * - Try different phrasings and see what works best
 * - Teach students about prompt engineering
 * - Adapt to different games (Zork, Leather Goddesses of Phobos, etc.)
 * - Support future use cases like LLM-to-LLM gameplay
 *
 * ## How It Fits In
 *
 * ```
 * Game starts → prompt_loader_init()
 *                 ↓
 *              Reads prompts/system.txt
 *              Reads prompts/user_template.txt
 *                 ↓
 * User input → prompt_loader_format_user_prompt()
 *                 ↓
 *              Substitutes {CONTEXT} and {INPUT}
 *                 ↓
 *              Returns formatted prompt → llm_client.c
 * ```
 *
 * ## Key Functions
 *
 * - `prompt_loader_init()` - Load prompts from files at startup
 * - `prompt_loader_format_user_prompt()` - Substitute placeholders
 * - `prompt_loader_get_system_prompt()` - Get system prompt
 * - `prompt_loader_shutdown()` - Free memory
 *
 * ## Example Usage
 *
 * ```c
 * // At startup
 * if (prompt_loader_init("prompts") != 0) {
 *     fprintf(stderr, "Warning: Using default prompts\n");
 * }
 *
 * // When calling LLM
 * const char *system = prompt_loader_get_system_prompt();
 * char user_prompt[4096];
 * prompt_loader_format_user_prompt(game_context, user_input, user_prompt, 4096);
 *
 * // Send to API: system + user_prompt
 * ```
 *
 * ## Future: LLM-to-LLM Gameplay
 *
 * This design supports having an LLM autonomously play the game:
 * - Same prompt loader provides instructions to the "player LLM"
 * - Different prompt files for different player types
 * - E.g., prompts/player_agent.txt vs prompts/translator.txt
 */

#ifndef TT_ZORK_PROMPT_LOADER_H
#define TT_ZORK_PROMPT_LOADER_H

#include <stddef.h>

/**
 * Initialize the prompt loader
 *
 * Attempts to load prompts from the specified directory.
 * Falls back to built-in defaults if files are missing.
 *
 * @param prompt_dir Path to prompts directory (e.g., "prompts")
 * @return 0 on success, -1 if using fallback defaults
 */
int prompt_loader_init(const char *prompt_dir);

/**
 * Get the system prompt
 *
 * Returns the LLM system prompt that defines its role and behavior.
 * This is typically sent once at the start of an API conversation.
 *
 * @return Pointer to system prompt string (do not free)
 */
const char *prompt_loader_get_system_prompt(void);

/**
 * Format a user prompt with context and input
 *
 * Takes the user template and substitutes:
 * - {CONTEXT} with recent game history
 * - {INPUT} with the user's natural language input
 *
 * @param context Game context string (can be NULL for empty context)
 * @param user_input User's natural language input
 * @param output Buffer to write formatted prompt
 * @param output_size Size of output buffer
 * @return 0 on success, -1 if output buffer too small
 */
int prompt_loader_format_user_prompt(const char *context,
                                      const char *user_input,
                                      char *output,
                                      size_t output_size);

/**
 * Shut down the prompt loader and free resources
 */
void prompt_loader_shutdown(void);

#endif /* TT_ZORK_PROMPT_LOADER_H */
