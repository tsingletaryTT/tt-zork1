/**
 * llm_client.h - HTTP Client for OpenAI-Compatible LLM APIs
 *
 * EDUCATIONAL PROJECT - READ THIS FIRST!
 *
 * ## What This Module Does
 *
 * This module makes HTTP requests to a language model API to translate natural
 * language into Zork commands. It's the "network brain" of our system.
 *
 * Think of it like making a phone call:
 * 1. Dial the number (API URL)
 * 2. Say what you want (send JSON request)
 * 3. Listen to the response (receive JSON)
 * 4. Hang up (close connection)
 *
 * ## Architecture Context
 *
 * ```
 * input_translator.c
 *        ↓
 *   "translate this"
 *        ↓
 * llm_client_translate() ← You are here!
 *        ↓
 *   [Build JSON request]
 *        ↓
 *   [HTTP POST via libcurl]
 *        ↓
 *   http://localhost:1234/v1/chat/completions
 *        ↓
 *   [LLM Server processes]
 *        ↓
 *   [Receive JSON response]
 *        ↓
 *   [Parse content]
 *        ↓
 *   Returns: "north, open door"
 * ```
 *
 * ## Why libcurl?
 *
 * libcurl is the industry-standard HTTP library for C:
 * - Used by billions of devices
 * - Rock-solid, well-tested
 * - Supports HTTPS, timeouts, error handling
 * - Cross-platform (works on native and RISC-V with effort)
 *
 * Alternatives we considered:
 * - Raw sockets: Too much work, no HTTPS
 * - wget/curl command: Messy to parse, adds dependency
 * - Custom HTTP: Reinventing the wheel
 *
 * ## OpenAI-Compatible APIs
 *
 * Many LLM servers support the OpenAI chat completion format:
 * - OpenAI (obviously)
 * - LM Studio (local)
 * - Ollama (local)
 * - LocalAI (local)
 * - Text Generation WebUI (local)
 *
 * This means you can:
 * - Develop locally with LM Studio
 * - Deploy with cloud OpenAI
 * - Run custom models with Ollama
 * - All with the same code!
 *
 * ## Configuration
 *
 * Set via environment variables:
 * - ZORK_LLM_URL: API endpoint (default: http://localhost:1234/v1/chat/completions)
 * - ZORK_LLM_MODEL: Model name (default: "zork-assistant")
 * - ZORK_LLM_API_KEY: API key if needed (optional)
 * - ZORK_LLM_ENABLED: Set to "0" to disable LLM (default: "1")
 *
 * ## Key Functions
 *
 * - `llm_client_init()` - Set up API configuration
 * - `llm_client_translate()` - Main API call
 * - `llm_client_is_enabled()` - Check if LLM is active
 * - `llm_client_shutdown()` - Clean up
 */

#ifndef TT_ZORK_LLM_CLIENT_H
#define TT_ZORK_LLM_CLIENT_H

#include <stddef.h>

/**
 * Initialize the LLM client
 *
 * Reads configuration from environment variables and sets up libcurl.
 * Call this once at startup.
 *
 * Environment variables:
 * - ZORK_LLM_URL: API endpoint URL
 * - ZORK_LLM_MODEL: Model name to use
 * - ZORK_LLM_API_KEY: API key (optional, for remote APIs)
 * - ZORK_LLM_ENABLED: "1" to enable, "0" to disable
 *
 * @return 0 on success, -1 on error (still usable if disabled)
 */
int llm_client_init(void);

/**
 * Translate natural language to Zork commands
 *
 * Sends user input and game context to LLM, receives translated commands.
 *
 * Example:
 *   context = "West of House. There is a mailbox here."
 *   user_input = "open it"
 *   → Returns: "open mailbox"
 *
 * @param system_prompt LLM system prompt (role definition)
 * @param user_prompt Formatted user message (context + input)
 * @param output Buffer for translated commands
 * @param output_size Size of output buffer
 * @param timeout_seconds HTTP timeout (0 = default 5 seconds)
 * @return 0 on success, -1 on failure
 */
int llm_client_translate(const char *system_prompt,
                          const char *user_prompt,
                          char *output,
                          size_t output_size,
                          int timeout_seconds);

/**
 * Check if LLM is enabled
 *
 * Returns false if:
 * - ZORK_LLM_ENABLED is "0"
 * - Initialization failed
 * - Configuration is invalid
 *
 * @return 1 if enabled, 0 if disabled
 */
int llm_client_is_enabled(void);

/**
 * Get last error message
 *
 * Returns human-readable description of last error.
 * Useful for debugging when requests fail.
 *
 * @return Error string (do not free)
 */
const char *llm_client_get_last_error(void);

/**
 * Shut down LLM client and free resources
 */
void llm_client_shutdown(void);

#endif /* TT_ZORK_LLM_CLIENT_H */
