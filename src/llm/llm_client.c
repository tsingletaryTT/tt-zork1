/**
 * llm_client.c - HTTP Client Implementation
 *
 * EDUCATIONAL PROJECT - Learn how HTTP APIs work!
 *
 * This implements HTTP communication with LLM APIs using libcurl.
 * It's a great example of network programming in C.
 *
 * ## HTTP Request/Response Cycle
 *
 * 1. **Setup**: Configure URL, headers, timeout
 * 2. **Request**: Send HTTP POST with JSON body
 * 3. **Response**: Receive data in chunks via callback
 * 4. **Parse**: Extract content from JSON
 * 5. **Cleanup**: Free resources
 *
 * ## libcurl Basics (for students)
 *
 * libcurl uses a handle-based API:
 *
 * ```c
 * CURL *curl = curl_easy_init();           // Create handle
 * curl_easy_setopt(curl, OPTION, value);   // Configure
 * curl_easy_perform(curl);                 // Execute request
 * curl_easy_cleanup(curl);                 // Free handle
 * ```
 *
 * ## Callback Pattern
 *
 * HTTP responses can be large, so libcurl delivers data in chunks
 * via a callback function. We accumulate chunks into a buffer.
 *
 * This is similar to:
 * - JavaScript fetch() with streaming
 * - Python requests with iter_content()
 * - Java InputStream reading
 *
 * ## Error Handling Philosophy
 *
 * Network calls can fail in many ways:
 * - Connection refused (server not running)
 * - Timeout (server too slow)
 * - HTTP errors (404, 500, etc.)
 * - Invalid JSON (malformed response)
 *
 * We handle all gracefully: log error, return -1, game continues.
 *
 * ## Future: RISC-V Support
 *
 * For RISC-V deployment, we'd need:
 * - Cross-compile libcurl for RISC-V
 * - OR implement minimal HTTP client
 * - OR use TT-Metal host communication channel
 *
 * Current design makes this modular - just swap implementation.
 */

#include "llm_client.h"
#include "json_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

/* Default configuration */
#define DEFAULT_API_URL "http://localhost:1234/v1/chat/completions"
#define DEFAULT_MODEL "zork-assistant"
#define DEFAULT_TIMEOUT 5
#define MAX_RESPONSE_SIZE (64 * 1024)  /* 64KB should be plenty */

/* Client configuration */
static struct {
    char api_url[512];
    char model[128];
    char api_key[256];
    int enabled;
    int initialized;
    int mock_mode;
    int mock_call_count;
    char last_error[512];
} config = {0};

/* Mock responses for testing without LLM server (first 4 screens) */
static const char *mock_responses[] = {
    "open mailbox",                          /* Screen 1: Opening game */
    "take leaflet, read leaflet",            /* Screen 2: Taking items */
    "go north",                              /* Screen 3: Moving around */
    "open window, enter house",              /* Screen 4: Entering house */
    NULL
};

/**
 * Response buffer for accumulating HTTP response data
 *
 * libcurl calls our callback multiple times with chunks of data.
 * We accumulate these chunks into this buffer.
 */
typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} response_buffer_t;

/**
 * Initialize response buffer
 */
static int response_buffer_init(response_buffer_t *buf, size_t initial_capacity) {
    buf->data = malloc(initial_capacity);
    if (!buf->data) {
        return -1;
    }
    buf->data[0] = '\0';
    buf->size = 0;
    buf->capacity = initial_capacity;
    return 0;
}

/**
 * Free response buffer
 */
static void response_buffer_free(response_buffer_t *buf) {
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->size = 0;
    buf->capacity = 0;
}

/**
 * libcurl write callback
 *
 * This function is called by libcurl whenever data arrives.
 * We append it to our response buffer.
 *
 * Callback signature required by libcurl:
 * @param ptr Pointer to received data
 * @param size Always 1 (historical reasons)
 * @param nmemb Number of bytes
 * @param userdata Our response_buffer_t pointer
 * @return Number of bytes processed (or 0 to abort)
 */
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    response_buffer_t *buf = (response_buffer_t *)userdata;
    size_t bytes_to_add = size * nmemb;

    /* Check if we need to grow the buffer */
    if (buf->size + bytes_to_add >= buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        if (new_capacity > MAX_RESPONSE_SIZE) {
            fprintf(stderr, "Error: Response too large\n");
            return 0; /* Abort transfer */
        }

        char *new_data = realloc(buf->data, new_capacity);
        if (!new_data) {
            fprintf(stderr, "Error: Out of memory for response\n");
            return 0; /* Abort transfer */
        }

        buf->data = new_data;
        buf->capacity = new_capacity;
    }

    /* Append new data */
    memcpy(buf->data + buf->size, ptr, bytes_to_add);
    buf->size += bytes_to_add;
    buf->data[buf->size] = '\0'; /* Null-terminate */

    return bytes_to_add;
}

/*
 * Public API Implementation
 */

int llm_client_init(void) {
    /* Read environment variables */
    const char *url = getenv("ZORK_LLM_URL");
    const char *model = getenv("ZORK_LLM_MODEL");
    const char *api_key = getenv("ZORK_LLM_API_KEY");
    const char *enabled_str = getenv("ZORK_LLM_ENABLED");
    const char *mock_str = getenv("ZORK_LLM_MOCK");

    /* Check if mock mode is enabled (for testing without LLM server) */
    if (mock_str && strcmp(mock_str, "1") == 0) {
        fprintf(stderr, "LLM client: Running in MOCK MODE (first 4 screens)\n");
        fprintf(stderr, "  Set ZORK_LLM_MOCK=0 to disable mock mode\n");
        config.enabled = 1;
        config.initialized = 1;
        config.mock_mode = 1;
        config.mock_call_count = 0;
        return 0;
    }

    /* Check if explicitly disabled */
    if (enabled_str && strcmp(enabled_str, "0") == 0) {
        fprintf(stderr, "LLM client: Disabled via ZORK_LLM_ENABLED=0\n");
        config.enabled = 0;
        config.initialized = 1;
        return -1;
    }

    /* Use defaults if not specified */
    strncpy(config.api_url, url ? url : DEFAULT_API_URL, sizeof(config.api_url) - 1);
    strncpy(config.model, model ? model : DEFAULT_MODEL, sizeof(config.model) - 1);

    if (api_key) {
        strncpy(config.api_key, api_key, sizeof(config.api_key) - 1);
    } else {
        config.api_key[0] = '\0';
    }

    /* Initialize libcurl globally */
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res != CURLE_OK) {
        snprintf(config.last_error, sizeof(config.last_error),
                 "Failed to initialize libcurl: %s", curl_easy_strerror(res));
        fprintf(stderr, "LLM client: %s\n", config.last_error);
        config.enabled = 0;
        config.initialized = 1;
        return -1;
    }

    config.enabled = 1;
    config.initialized = 1;

    fprintf(stderr, "LLM client: Initialized\n");
    fprintf(stderr, "  URL: %s\n", config.api_url);
    fprintf(stderr, "  Model: %s\n", config.model);
    fprintf(stderr, "  API Key: %s\n", config.api_key[0] ? "[set]" : "[none]");

    return 0;
}

int llm_client_translate(const char *system_prompt,
                          const char *user_prompt,
                          char *output,
                          size_t output_size,
                          int timeout_seconds) {
    if (!config.initialized) {
        llm_client_init(); /* Auto-initialize if needed */
    }

    if (!config.enabled) {
        snprintf(config.last_error, sizeof(config.last_error),
                 "LLM client is disabled");
        return -1;
    }

    if (!system_prompt || !user_prompt || !output || output_size == 0) {
        snprintf(config.last_error, sizeof(config.last_error),
                 "Invalid parameters");
        return -1;
    }

    /* Mock mode: return canned responses for first 4 calls */
    if (config.mock_mode) {
        if (config.mock_call_count < 4 && mock_responses[config.mock_call_count]) {
            strncpy(output, mock_responses[config.mock_call_count], output_size - 1);
            output[output_size - 1] = '\0';
            config.mock_call_count++;
            fprintf(stderr, "[MOCK] Returned response #%d: %s\n",
                    config.mock_call_count, output);
            return 0;
        } else {
            /* After 4 calls, fall back to pass-through */
            fprintf(stderr, "[MOCK] Exceeded mock limit, falling back to pass-through\n");
            snprintf(config.last_error, sizeof(config.last_error),
                     "Mock mode exhausted");
            return -1;
        }
    }

    /* Build JSON request */
    char *request_json = malloc(128 * 1024); /* 128KB for request */
    if (!request_json) {
        snprintf(config.last_error, sizeof(config.last_error),
                 "Out of memory");
        return -1;
    }

    if (json_build_chat_request(config.model, system_prompt, user_prompt,
                                 0.7, 100, request_json, 128 * 1024) != 0) {
        free(request_json);
        snprintf(config.last_error, sizeof(config.last_error),
                 "Failed to build JSON request");
        return -1;
    }

    /* Initialize curl handle */
    CURL *curl = curl_easy_init();
    if (!curl) {
        free(request_json);
        snprintf(config.last_error, sizeof(config.last_error),
                 "Failed to create curl handle");
        return -1;
    }

    /* Initialize response buffer */
    response_buffer_t response;
    if (response_buffer_init(&response, 4096) != 0) {
        curl_easy_cleanup(curl);
        free(request_json);
        snprintf(config.last_error, sizeof(config.last_error),
                 "Out of memory for response buffer");
        return -1;
    }

    /* Set curl options */
    curl_easy_setopt(curl, CURLOPT_URL, config.api_url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_json);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds ? timeout_seconds : DEFAULT_TIMEOUT);

    /* Set headers */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (config.api_key[0] != '\0') {
        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header),
                 "Authorization: Bearer %s", config.api_key);
        headers = curl_slist_append(headers, auth_header);
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    /* Perform request */
    CURLcode res = curl_easy_perform(curl);

    /* Check HTTP status code */
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    /* Cleanup */
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(request_json);

    /* Check for errors */
    if (res != CURLE_OK) {
        snprintf(config.last_error, sizeof(config.last_error),
                 "HTTP request failed: %s", curl_easy_strerror(res));
        response_buffer_free(&response);
        return -1;
    }

    if (http_code != 200) {
        snprintf(config.last_error, sizeof(config.last_error),
                 "HTTP error %ld", http_code);
        response_buffer_free(&response);
        return -1;
    }

    /* Parse JSON response */
    if (json_parse_content(response.data, output, output_size) != 0) {
        snprintf(config.last_error, sizeof(config.last_error),
                 "Failed to parse JSON response");
        response_buffer_free(&response);
        return -1;
    }

    response_buffer_free(&response);
    return 0;
}

int llm_client_is_enabled(void) {
    if (!config.initialized) {
        llm_client_init();
    }
    return config.enabled;
}

const char *llm_client_get_last_error(void) {
    return config.last_error;
}

void llm_client_shutdown(void) {
    if (config.initialized) {
        curl_global_cleanup();
        config.initialized = 0;
    }
}
