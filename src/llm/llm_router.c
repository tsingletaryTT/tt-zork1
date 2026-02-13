/**
 * llm_router.c - Mode-Agnostic LLM Request Router Implementation
 *
 * This implements the flexible architecture that supports multiple LLM
 * deployment modes with a single codebase.
 *
 * ## Implementation Strategy
 *
 * 1. **Configuration Loading**: Parse YAML config to determine mode
 * 2. **Endpoint Management**: Store URLs/models/prompts per task
 * 3. **Request Routing**: Direct to appropriate endpoint based on task+mode
 * 4. **Prompt Handling**: Load and cache role-specific prompts
 *
 * ## Data Structures
 *
 * ```
 * RouterConfig
 *   ├─ mode (multi_agent, unified, hybrid, etc.)
 *   ├─ endpoints[4]  // One per task type
 *   │   ├─ url
 *   │   ├─ model
 *   │   ├─ prompt_file
 *   │   ├─ max_tokens
 *   │   └─ temperature
 *   └─ unified_config  // For unified mode
 *       ├─ url
 *       ├─ model
 *       └─ role_prompts[4]
 * ```
 *
 * ## YAML Parsing Approach
 *
 * We use a minimal YAML parser suitable for our simple config format:
 * - Key-value pairs: "key: value"
 * - Nested sections with indentation
 * - Lists: "chips: [0, 1, 2]"
 * - Comments: "# comment"
 *
 * This avoids heavy dependencies while supporting our needs.
 */

#include "llm_router.h"
#include "llm_client.h"
#include "prompt_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Maximum configuration sizes */
#define MAX_URL_LENGTH 512
#define MAX_PATH_LENGTH 256
#define MAX_MODEL_NAME 256
#define MAX_LINE_LENGTH 1024
#define MAX_PROMPT_SIZE 4096
#define MAX_ERROR_LENGTH 512

/**
 * Endpoint configuration for a specific task
 */
typedef struct {
    char url[MAX_URL_LENGTH];              /** API endpoint URL */
    char model[MAX_MODEL_NAME];            /** Model name */
    char prompt_file[MAX_PATH_LENGTH];     /** Path to prompt file */
    int max_tokens;                        /** Max completion tokens */
    float temperature;                     /** Sampling temperature */
    int configured;                        /** 1 if this endpoint is set up */
    char cached_prompt[MAX_PROMPT_SIZE];   /** Cached system prompt */
} EndpointConfig;

/**
 * Global router configuration
 */
typedef struct {
    LLMMode mode;                          /** Current deployment mode */

    /* Multi-agent mode: separate endpoint per task */
    EndpointConfig endpoints[4];           /** Indexed by LLMTaskType */

    /* Unified mode: single endpoint with role prompts */
    EndpointConfig unified_endpoint;       /** Shared endpoint for all tasks */
    char role_prompts[4][MAX_PROMPT_SIZE]; /** Role-specific prompt prefixes */

    int initialized;                       /** 1 if successfully initialized */
    char last_error[MAX_ERROR_LENGTH];     /** Last error message */
} RouterConfig;

/* Global configuration */
static RouterConfig g_router_config = {0};

/* Forward declarations */
static int parse_config_file(const char *config_file);
static int load_endpoint_prompts(void);
static int route_multi_agent(LLMTaskType task, const char *input,
                               char *output, size_t output_size);
static int route_unified(LLMTaskType task, const char *input,
                          char *output, size_t output_size);
static void set_error(const char *error_msg);
static char *trim_whitespace(char *str);
static LLMMode string_to_mode(const char *mode_str);

/**
 * Initialize LLM router
 */
int llm_router_init(const char *config_file) {
    /* Default config file */
    const char *default_config = "config/llm_mode.yaml";
    if (config_file == NULL) {
        config_file = default_config;
    }

    /* Reset configuration */
    memset(&g_router_config, 0, sizeof(g_router_config));

    /* Parse configuration file */
    if (parse_config_file(config_file) != 0) {
        set_error("Failed to parse configuration file");
        return -1;
    }

    /* Validate mode was set */
    if (g_router_config.mode == LLM_MODE_UNKNOWN) {
        set_error("No valid mode found in configuration");
        return -1;
    }

    /* Load prompts for configured endpoints */
    if (load_endpoint_prompts() != 0) {
        set_error("Failed to load prompts");
        return -1;
    }

    /* Initialize underlying LLM client */
    llm_client_init();

    g_router_config.initialized = 1;
    return 0;
}

/**
 * Route LLM request based on task type and mode
 */
int llm_router_request(LLMTaskType task,
                        const char *input,
                        char *output,
                        size_t output_size) {
    /* Validate initialization */
    if (!g_router_config.initialized) {
        set_error("Router not initialized");
        return -1;
    }

    /* Validate parameters */
    if (input == NULL || output == NULL || output_size == 0) {
        set_error("Invalid parameters");
        return -1;
    }

    /* Route based on mode */
    switch (g_router_config.mode) {
        case LLM_MODE_MULTI_AGENT:
        case LLM_MODE_HYBRID_IMAGE:
        case LLM_MODE_RESEARCH:
            return route_multi_agent(task, input, output, output_size);

        case LLM_MODE_UNIFIED:
        case LLM_MODE_MINIMAL:
            return route_unified(task, input, output, output_size);

        default:
            set_error("Unknown routing mode");
            return -1;
    }
}

/**
 * Get current LLM mode
 */
LLMMode llm_router_get_mode(void) {
    return g_router_config.mode;
}

/**
 * Get human-readable mode name
 */
const char *llm_router_mode_to_string(LLMMode mode) {
    switch (mode) {
        case LLM_MODE_MULTI_AGENT:   return "multi_agent";
        case LLM_MODE_UNIFIED:       return "unified";
        case LLM_MODE_HYBRID_IMAGE:  return "hybrid_image";
        case LLM_MODE_MINIMAL:       return "minimal";
        case LLM_MODE_RESEARCH:      return "research";
        default:                     return "unknown";
    }
}

/**
 * Get configuration info for a task
 */
int llm_router_get_task_info(LLMTaskType task,
                               char *info_buffer,
                               size_t buffer_size) {
    if (!g_router_config.initialized || info_buffer == NULL) {
        return -1;
    }

    const char *task_names[] = {"translator", "artist", "dm", "player"};
    const char *task_name = task_names[task];

    if (g_router_config.mode == LLM_MODE_MULTI_AGENT) {
        EndpointConfig *ep = &g_router_config.endpoints[task];
        snprintf(info_buffer, buffer_size,
                 "Task: %s\nMode: multi_agent\nURL: %s\nModel: %s\n"
                 "Tokens: %d, Temp: %.2f",
                 task_name, ep->url, ep->model, ep->max_tokens, ep->temperature);
    } else {
        EndpointConfig *ep = &g_router_config.unified_endpoint;
        snprintf(info_buffer, buffer_size,
                 "Task: %s\nMode: unified\nURL: %s\nModel: %s\n"
                 "Tokens: %d, Temp: %.2f",
                 task_name, ep->url, ep->model, ep->max_tokens, ep->temperature);
    }

    return 0;
}

/**
 * Check if router is ready
 */
int llm_router_is_ready(void) {
    return g_router_config.initialized;
}

/**
 * Get last error message
 */
const char *llm_router_get_last_error(void) {
    return g_router_config.last_error;
}

/**
 * Reload configuration
 */
int llm_router_reload(void) {
    /* Save old config in case reload fails */
    RouterConfig old_config = g_router_config;

    /* Try to initialize with new config */
    if (llm_router_init(NULL) != 0) {
        /* Restore old config */
        g_router_config = old_config;
        return -1;
    }

    return 0;
}

/**
 * Shut down router
 */
void llm_router_shutdown(void) {
    memset(&g_router_config, 0, sizeof(g_router_config));
}

/* ============================================================================
 * INTERNAL FUNCTIONS
 * ============================================================================ */

/**
 * Parse configuration file (minimal YAML parser)
 */
static int parse_config_file(const char *config_file) {
    FILE *f = fopen(config_file, "r");
    if (!f) {
        char error[256];
        snprintf(error, sizeof(error), "Cannot open config file: %s", config_file);
        set_error(error);
        return -1;
    }

    char line[MAX_LINE_LENGTH];
    char current_section[128] = {0};
    char current_endpoint[128] = {0};
    int in_endpoints = 0;

    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim_whitespace(line);

        /* Skip empty lines and comments */
        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            continue;
        }

        /* Parse mode line */
        if (strncmp(trimmed, "mode:", 5) == 0) {
            char *mode_str = trim_whitespace(trimmed + 5);
            g_router_config.mode = string_to_mode(mode_str);
            continue;
        }

        /* Track sections */
        if (strncmp(trimmed, "endpoints:", 10) == 0) {
            in_endpoints = 1;
            continue;
        }

        /* Parse endpoint entries */
        if (in_endpoints && strstr(trimmed, ":") && !strstr(trimmed, "  ")) {
            /* New endpoint name (translator:, artist:, etc.) */
            char *colon = strchr(trimmed, ':');
            if (colon) {
                *colon = '\0';
                strncpy(current_endpoint, trimmed, sizeof(current_endpoint) - 1);
            }
            continue;
        }

        /* Parse endpoint properties */
        if (in_endpoints && current_endpoint[0] != '\0') {
            /* Determine which endpoint to configure */
            EndpointConfig *ep = NULL;
            LLMTaskType task_idx = -1;

            if (strcmp(current_endpoint, "translator") == 0) {
                task_idx = LLM_TASK_TRANSLATE;
                ep = &g_router_config.endpoints[task_idx];
            } else if (strcmp(current_endpoint, "artist") == 0) {
                task_idx = LLM_TASK_VISUALIZE;
                ep = &g_router_config.endpoints[task_idx];
            } else if (strcmp(current_endpoint, "dm") == 0) {
                task_idx = LLM_TASK_NARRATE;
                ep = &g_router_config.endpoints[task_idx];
            } else if (strcmp(current_endpoint, "player") == 0) {
                task_idx = LLM_TASK_AUTOPLAY;
                ep = &g_router_config.endpoints[task_idx];
            } else if (strcmp(current_endpoint, "all_tasks") == 0) {
                /* Unified mode endpoint */
                ep = &g_router_config.unified_endpoint;
            }

            if (ep) {
                /* Parse property */
                if (strstr(trimmed, "url:")) {
                    char *value = strchr(trimmed, ':');
                    if (value) {
                        strncpy(ep->url, trim_whitespace(value + 1),
                                sizeof(ep->url) - 1);
                    }
                } else if (strstr(trimmed, "model:")) {
                    char *value = strchr(trimmed, ':');
                    if (value) {
                        strncpy(ep->model, trim_whitespace(value + 1),
                                sizeof(ep->model) - 1);
                    }
                } else if (strstr(trimmed, "prompt_file:")) {
                    char *value = strchr(trimmed, ':');
                    if (value) {
                        strncpy(ep->prompt_file, trim_whitespace(value + 1),
                                sizeof(ep->prompt_file) - 1);
                    }
                } else if (strstr(trimmed, "max_tokens:")) {
                    char *value = strchr(trimmed, ':');
                    if (value) {
                        ep->max_tokens = atoi(trim_whitespace(value + 1));
                    }
                } else if (strstr(trimmed, "temperature:")) {
                    char *value = strchr(trimmed, ':');
                    if (value) {
                        ep->temperature = atof(trim_whitespace(value + 1));
                    }
                }
                ep->configured = 1;
            }
        }
    }

    fclose(f);
    return 0;
}

/**
 * Load prompts for all configured endpoints
 */
static int load_endpoint_prompts(void) {
    /* Multi-agent mode: load prompts for each endpoint */
    if (g_router_config.mode == LLM_MODE_MULTI_AGENT ||
        g_router_config.mode == LLM_MODE_HYBRID_IMAGE ||
        g_router_config.mode == LLM_MODE_RESEARCH) {

        for (int i = 0; i < 4; i++) {
            EndpointConfig *ep = &g_router_config.endpoints[i];
            if (ep->configured && ep->prompt_file[0] != '\0') {
                if (prompt_loader_load(ep->prompt_file,
                                        ep->cached_prompt,
                                        sizeof(ep->cached_prompt)) != 0) {
                    char error[256];
                    snprintf(error, sizeof(error),
                             "Failed to load prompt: %s", ep->prompt_file);
                    set_error(error);
                    return -1;
                }
            }
        }
    }

    /* Unified mode: load single prompt (role-specific prompts handled separately) */
    if (g_router_config.mode == LLM_MODE_UNIFIED ||
        g_router_config.mode == LLM_MODE_MINIMAL) {

        EndpointConfig *ep = &g_router_config.unified_endpoint;
        if (ep->configured && ep->prompt_file[0] != '\0') {
            if (prompt_loader_load(ep->prompt_file,
                                    ep->cached_prompt,
                                    sizeof(ep->cached_prompt)) != 0) {
                set_error("Failed to load unified prompt");
                return -1;
            }
        }
    }

    return 0;
}

/**
 * Route request in multi-agent mode
 */
static int route_multi_agent(LLMTaskType task, const char *input,
                               char *output, size_t output_size) {
    /* Get endpoint for this task */
    EndpointConfig *ep = &g_router_config.endpoints[task];

    if (!ep->configured) {
        set_error("Endpoint not configured for this task");
        return -1;
    }

    /* Temporarily override llm_client configuration for this endpoint */
    /* (In production, we'd use a more elegant per-request config) */
    char old_url[512];
    const char *env_url = getenv("ZORK_LLM_URL");
    if (env_url) {
        strncpy(old_url, env_url, sizeof(old_url) - 1);
    }

    /* Set endpoint-specific URL */
    setenv("ZORK_LLM_URL", ep->url, 1);
    setenv("ZORK_LLM_MODEL", ep->model, 1);

    /* Make API call with endpoint's prompt */
    int result = llm_client_translate(ep->cached_prompt, input,
                                       output, output_size, 0);

    /* Restore original URL */
    if (env_url) {
        setenv("ZORK_LLM_URL", old_url, 1);
    }

    return result;
}

/**
 * Route request in unified mode
 */
static int route_unified(LLMTaskType task, const char *input,
                          char *output, size_t output_size) {
    EndpointConfig *ep = &g_router_config.unified_endpoint;

    if (!ep->configured) {
        set_error("Unified endpoint not configured");
        return -1;
    }

    /* In unified mode, we add role-specific prefix to the prompt */
    /* For now, we'll use the base prompt - role prompts are a future enhancement */

    /* Set endpoint configuration */
    setenv("ZORK_LLM_URL", ep->url, 1);
    setenv("ZORK_LLM_MODEL", ep->model, 1);

    /* Make API call */
    int result = llm_client_translate(ep->cached_prompt, input,
                                       output, output_size, 0);

    return result;
}

/**
 * Set error message
 */
static void set_error(const char *error_msg) {
    strncpy(g_router_config.last_error, error_msg,
            sizeof(g_router_config.last_error) - 1);
}

/**
 * Trim leading/trailing whitespace
 */
static char *trim_whitespace(char *str) {
    /* Trim leading */
    while (isspace((unsigned char)*str)) str++;

    /* Trim trailing */
    char *end = str + strlen(str) - 1;
    while (end > str && (isspace((unsigned char)*end) || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }

    return str;
}

/**
 * Convert mode string to enum
 */
static LLMMode string_to_mode(const char *mode_str) {
    if (strcmp(mode_str, "multi_agent") == 0) {
        return LLM_MODE_MULTI_AGENT;
    } else if (strcmp(mode_str, "unified") == 0) {
        return LLM_MODE_UNIFIED;
    } else if (strcmp(mode_str, "hybrid_image") == 0) {
        return LLM_MODE_HYBRID_IMAGE;
    } else if (strcmp(mode_str, "minimal") == 0) {
        return LLM_MODE_MINIMAL;
    } else if (strcmp(mode_str, "research") == 0) {
        return LLM_MODE_RESEARCH;
    }
    return LLM_MODE_UNKNOWN;
}
