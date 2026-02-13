/**
 * llm_router.h - Mode-Agnostic LLM Request Router
 *
 * FLEXIBLE ARCHITECTURE - Tech Demo Feature!
 *
 * ## What This Module Does
 *
 * This router enables switching between different LLM deployment architectures
 * WITHOUT changing application code. Think of it as a "universal adapter" that
 * works with any backend configuration.
 *
 * ## Supported Modes
 *
 * **Multi-Agent Mode** (4 small models, one per chip):
 * ```
 * User → Router → [Translator] [Artist] [DM] [Player]
 *                  Chip 0       Chip 1    Chip 2  Chip 3
 *                  :8000        :8001     :8002   :8003
 * ```
 *
 * **Unified Mode** (1 large model, tensor parallel):
 * ```
 * User → Router → [Qwen3-32B (all roles via prompts)]
 *                  Chips 0-3 (tensor parallel)
 *                  :9000
 * ```
 *
 * **Hybrid Mode** (example: 2 chips text, 2 chips image):
 * ```
 * User → Router → [Qwen3-8B]  [SDXL]
 *                  Chips 0-1   Chips 2-3
 *                  :8000       :8100
 * ```
 *
 * ## How It Works
 *
 * ```
 * Application calls:
 *   llm_router_request(TRANSLATE, "open the door", buffer, size)
 *
 * Router checks mode:
 *
 * Multi-Agent Mode:
 *   → Routes to translator endpoint (http://localhost:8000)
 *   → Uses translator-specific prompts
 *   → Returns translated command
 *
 * Unified Mode:
 *   → Routes to unified endpoint (http://localhost:9000)
 *   → Prefixes with translator role prompt
 *   → Returns translated command
 *
 * Application code stays the SAME!
 * ```
 *
 * ## Configuration
 *
 * Reads from `config/llm_mode.yaml`:
 * - Mode selection (multi_agent, unified, hybrid_image, etc.)
 * - Endpoint URLs and ports
 * - Model names and chip assignments
 * - Prompt file paths
 * - Parameters (temperature, max_tokens, etc.)
 *
 * ## Task Types
 *
 * - TRANSLATE: Natural language → Zork commands (translator)
 * - VISUALIZE: Room description → ASCII art (artist)
 * - NARRATE: Standard text → Enhanced description (DM)
 * - AUTOPLAY: Game state → Suggested command (AI player)
 *
 * ## Benefits for Quietbox 2 Demo
 *
 * - Show architectural flexibility
 * - Switch modes without recompiling
 * - Compare 4-model vs 1-model performance
 * - Demonstrate tensor parallelism vs multi-agent
 * - Support arbitrary chip/model configurations
 *
 * ## Design Philosophy
 *
 * The router abstracts deployment details:
 * - Application: "I need to translate this"
 * - Router: "I'll handle it based on current configuration"
 * - Result: Clean separation of concerns
 *
 * ## Key Functions
 *
 * - `llm_router_init()` - Load config and detect mode
 * - `llm_router_request()` - Route request based on task type
 * - `llm_router_get_mode()` - Query current mode
 * - `llm_router_shutdown()` - Clean up resources
 */

#ifndef TT_ZORK_LLM_ROUTER_H
#define TT_ZORK_LLM_ROUTER_H

#include <stddef.h>

/**
 * LLM deployment modes
 */
typedef enum {
    LLM_MODE_UNKNOWN = 0,      /** Configuration not loaded or invalid */
    LLM_MODE_MULTI_AGENT,      /** 4 small models, specialized per task */
    LLM_MODE_UNIFIED,          /** 1 large model, role-based prompts */
    LLM_MODE_HYBRID_IMAGE,     /** Mixed: text models + image models */
    LLM_MODE_MINIMAL,          /** Single chip, shared endpoint */
    LLM_MODE_RESEARCH          /** Custom mixed configuration */
} LLMMode;

/**
 * Task types for routing decisions
 */
typedef enum {
    LLM_TASK_TRANSLATE = 0,    /** Natural language → Zork commands */
    LLM_TASK_VISUALIZE,        /** Room description → ASCII art */
    LLM_TASK_NARRATE,          /** Standard text → Enhanced description */
    LLM_TASK_AUTOPLAY          /** Game state → AI player command */
} LLMTaskType;

/**
 * Initialize LLM router
 *
 * Reads configuration from file, determines mode, validates endpoints.
 * Must be called before any other router functions.
 *
 * Initialization steps:
 * 1. Load config/llm_mode.yaml
 * 2. Parse mode setting
 * 3. Load endpoint configurations
 * 4. Load role-specific prompts (if unified mode)
 * 5. Validate all settings
 *
 * @param config_file Path to config file (NULL = default "config/llm_mode.yaml")
 * @return 0 on success, -1 on error
 */
int llm_router_init(const char *config_file);

/**
 * Route LLM request based on task type and current mode
 *
 * This is the main API for making LLM requests. The router automatically:
 * - Selects appropriate endpoint based on mode
 * - Loads correct prompts for the task
 * - Formats request appropriately
 * - Makes API call via llm_client
 * - Returns response
 *
 * In multi-agent mode:
 *   TRANSLATE → translator endpoint (chip 0, port 8000)
 *   VISUALIZE → artist endpoint (chip 1, port 8001)
 *   NARRATE → dm endpoint (chip 2, port 8002)
 *   AUTOPLAY → player endpoint (chip 3, port 8003)
 *
 * In unified mode:
 *   All tasks → unified endpoint (all chips, port 9000)
 *   Role-specific prompt prefix added automatically
 *
 * Example:
 *   char output[512];
 *   int result = llm_router_request(
 *       LLM_TASK_TRANSLATE,
 *       "I want to open the mailbox",
 *       output,
 *       sizeof(output)
 *   );
 *   // output now contains: "open mailbox"
 *
 * @param task Task type (determines routing)
 * @param input User input or game context
 * @param output Buffer for LLM response
 * @param output_size Size of output buffer
 * @return 0 on success, -1 on error
 */
int llm_router_request(LLMTaskType task,
                        const char *input,
                        char *output,
                        size_t output_size);

/**
 * Get current LLM mode
 *
 * Useful for:
 * - Display in UI ("Running in Multi-Agent mode")
 * - Conditional features (image gen only in hybrid mode)
 * - Debugging and logging
 *
 * @return Current mode (LLM_MODE_UNKNOWN if not initialized)
 */
LLMMode llm_router_get_mode(void);

/**
 * Get human-readable mode name
 *
 * @param mode Mode to convert
 * @return String name (e.g., "multi_agent", "unified")
 */
const char *llm_router_mode_to_string(LLMMode mode);

/**
 * Get configuration info for a specific task
 *
 * Useful for debugging - shows which endpoint/model handles this task.
 *
 * @param task Task type
 * @param info_buffer Buffer for info string
 * @param buffer_size Size of buffer
 * @return 0 on success, -1 on error
 */
int llm_router_get_task_info(LLMTaskType task,
                               char *info_buffer,
                               size_t buffer_size);

/**
 * Check if router is properly initialized
 *
 * Returns false if:
 * - Init not called yet
 * - Config file missing/invalid
 * - Mode unknown
 * - No endpoints configured
 *
 * @return 1 if ready, 0 if not initialized
 */
int llm_router_is_ready(void);

/**
 * Get last error message
 *
 * Returns human-readable description of last error.
 *
 * @return Error string (do not free), or NULL if no error
 */
const char *llm_router_get_last_error(void);

/**
 * Reload configuration from file
 *
 * Useful for switching modes without restarting:
 * 1. Edit config/llm_mode.yaml
 * 2. Call llm_router_reload()
 * 3. Next request uses new configuration
 *
 * @return 0 on success, -1 on error (old config still active)
 */
int llm_router_reload(void);

/**
 * Shut down router and free resources
 *
 * Cleans up:
 * - Loaded configuration
 * - Cached prompts
 * - Memory allocations
 */
void llm_router_shutdown(void);

#endif /* TT_ZORK_LLM_ROUTER_H */
