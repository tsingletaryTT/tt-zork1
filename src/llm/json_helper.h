/**
 * json_helper.h - Minimal JSON Utilities for OpenAI API
 *
 * EDUCATIONAL PROJECT - READ THIS FIRST!
 *
 * ## What This Module Does
 *
 * This provides *minimal* JSON handling for our specific use case: building
 * OpenAI chat completion requests and parsing responses. It's NOT a full
 * JSON library - just enough to get the job done.
 *
 * ## Why Minimal?
 *
 * We could use a full JSON library like cJSON or jansson, but:
 * 1. **Educational**: Shows you can solve problems with simple code
 * 2. **Dependencies**: Fewer dependencies = easier to port to RISC-V later
 * 3. **Size**: Full JSON parsers are 1000s of lines; we need ~200
 * 4. **Specific**: Our JSON is very predictable (OpenAI format)
 *
 * ## What We Support
 *
 * **Building**:
 * - OpenAI chat completion request format
 * - Escaping special characters in strings
 *
 * **Parsing**:
 * - Extract "content" field from response
 * - Handle nested structure: choices[0].message.content
 *
 * **What We DON'T Support**:
 * - Arbitrary JSON structures
 * - Arrays beyond choices[0]
 * - Unicode escapes beyond basics
 * - Pretty-printing
 * - Validation beyond basic syntax
 *
 * ## OpenAI API Format (What We're Building)
 *
 * Request:
 * ```json
 * {
 *   "model": "gpt-3.5-turbo",
 *   "messages": [
 *     {"role": "system", "content": "You are..."},
 *     {"role": "user", "content": "Translate..."}
 *   ],
 *   "temperature": 0.7,
 *   "max_tokens": 100
 * }
 * ```
 *
 * Response:
 * ```json
 * {
 *   "choices": [
 *     {"message": {"content": "north, open door"}}
 *   ]
 * }
 * ```
 *
 * We only care about extracting "north, open door".
 *
 * ## Key Functions
 *
 * - `json_escape_string()` - Escape special chars for JSON strings
 * - `json_build_chat_request()` - Build OpenAI request
 * - `json_parse_content()` - Extract content from response
 */

#ifndef TT_ZORK_JSON_HELPER_H
#define TT_ZORK_JSON_HELPER_H

#include <stddef.h>

/**
 * Escape a string for JSON
 *
 * Converts:  He said "Hello\n"
 * To:        He said \"Hello\\n\"
 *
 * Handles: " \ \n \t \r
 *
 * @param input String to escape
 * @param output Buffer for escaped string
 * @param output_size Size of output buffer
 * @return 0 on success, -1 if buffer too small
 */
int json_escape_string(const char *input, char *output, size_t output_size);

/**
 * Build an OpenAI chat completion request
 *
 * Creates JSON for OpenAI-compatible APIs.
 *
 * @param model Model name (e.g., "gpt-3.5-turbo")
 * @param system_prompt System message content
 * @param user_prompt User message content
 * @param temperature Sampling temperature (0.0-2.0)
 * @param max_tokens Maximum response length
 * @param output Buffer for JSON request
 * @param output_size Size of output buffer
 * @return 0 on success, -1 on error
 */
int json_build_chat_request(const char *model,
                             const char *system_prompt,
                             const char *user_prompt,
                             float temperature,
                             int max_tokens,
                             char *output,
                             size_t output_size);

/**
 * Parse content from OpenAI response
 *
 * Extracts: choices[0].message.content
 *
 * Example input:
 *   {"choices":[{"message":{"content":"north, open door"}}]}
 *
 * Extracts: "north, open door"
 *
 * @param json_response Full JSON response from API
 * @param output Buffer for extracted content
 * @param output_size Size of output buffer
 * @return 0 on success, -1 if parse failed
 */
int json_parse_content(const char *json_response, char *output, size_t output_size);

#endif /* TT_ZORK_JSON_HELPER_H */
