/**
 * json_helper.c - Minimal JSON Implementation
 *
 * EDUCATIONAL PROJECT - Learn how JSON works!
 *
 * This implements JSON handling using simple string operations. It's a great
 * example of solving a problem without heavy dependencies.
 *
 * ## JSON Format Basics (for students)
 *
 * JSON is just text with specific syntax:
 * - Objects: {"key": "value"}
 * - Arrays: [item1, item2]
 * - Strings: "text" (must escape " and \)
 * - Numbers: 123 or 45.67
 * - Booleans: true, false
 * - Null: null
 *
 * We only need objects, strings, numbers, and arrays.
 *
 * ## Our Approach
 *
 * **Building**: Use sprintf to construct JSON strings
 * - Pro: Simple, fast, no parsing needed
 * - Con: Must carefully escape strings
 *
 * **Parsing**: Use strstr to find patterns
 * - Pro: Works for our predictable format
 * - Con: Won't work for arbitrary JSON
 *
 * This is fine because OpenAI responses are consistent!
 *
 * ## What Would a Real JSON Library Do?
 *
 * Libraries like cJSON:
 * 1. Parse into tree structure (objects, arrays, values)
 * 2. Navigate tree with API calls
 * 3. Handle all edge cases (nested arrays, unicode, etc.)
 * 4. ~3000+ lines of code
 *
 * Us:
 * 1. Build strings directly
 * 2. Search for specific patterns
 * 3. Handle only what we need
 * 4. ~200 lines of code
 *
 * Choose the right tool for the job!
 */

#include "json_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/**
 * Escape special characters for JSON strings
 *
 * JSON strings require escaping these characters:
 * - " → \"
 * - \ → \\
 * - \n → \\n
 * - \t → \\t
 * - \r → \\r
 */
int json_escape_string(const char *input, char *output, size_t output_size) {
    if (!input || !output || output_size == 0) {
        return -1;
    }

    size_t out_pos = 0;
    const char *in_pos = input;

    while (*in_pos && out_pos < output_size - 2) {
        /* Check if character needs escaping */
        switch (*in_pos) {
            case '"':
                output[out_pos++] = '\\';
                if (out_pos >= output_size - 1) goto truncate;
                output[out_pos++] = '"';
                break;
            case '\\':
                output[out_pos++] = '\\';
                if (out_pos >= output_size - 1) goto truncate;
                output[out_pos++] = '\\';
                break;
            case '\n':
                output[out_pos++] = '\\';
                if (out_pos >= output_size - 1) goto truncate;
                output[out_pos++] = 'n';
                break;
            case '\t':
                output[out_pos++] = '\\';
                if (out_pos >= output_size - 1) goto truncate;
                output[out_pos++] = 't';
                break;
            case '\r':
                output[out_pos++] = '\\';
                if (out_pos >= output_size - 1) goto truncate;
                output[out_pos++] = 'r';
                break;
            default:
                /* Regular character - copy as-is */
                output[out_pos++] = *in_pos;
                break;
        }
        in_pos++;
    }

    output[out_pos] = '\0';
    return 0;

truncate:
    output[output_size - 1] = '\0';
    fprintf(stderr, "Warning: JSON string truncated during escaping\n");
    return -1;
}

/**
 * Build OpenAI chat completion request
 *
 * This constructs the JSON by sprintf-ing parts together.
 * Simple but effective!
 *
 * Format (minified for network efficiency):
 * {"model":"...","messages":[{"role":"system","content":"..."},
 * {"role":"user","content":"..."}],"temperature":0.7,"max_tokens":100}
 */
int json_build_chat_request(const char *model,
                             const char *system_prompt,
                             const char *user_prompt,
                             float temperature,
                             int max_tokens,
                             char *output,
                             size_t output_size) {
    if (!model || !system_prompt || !user_prompt || !output || output_size == 0) {
        return -1;
    }

    /* Allocate temporary buffers for escaped strings */
    char *esc_system = malloc(strlen(system_prompt) * 2 + 1);
    char *esc_user = malloc(strlen(user_prompt) * 2 + 1);

    if (!esc_system || !esc_user) {
        fprintf(stderr, "Error: Out of memory building JSON\n");
        free(esc_system);
        free(esc_user);
        return -1;
    }

    /* Escape the strings */
    if (json_escape_string(system_prompt, esc_system, strlen(system_prompt) * 2 + 1) != 0 ||
        json_escape_string(user_prompt, esc_user, strlen(user_prompt) * 2 + 1) != 0) {
        free(esc_system);
        free(esc_user);
        return -1;
    }

    /* Build JSON (minified - no extra whitespace) */
    int written = snprintf(output, output_size,
        "{"
          "\"model\":\"%s\","
          "\"messages\":["
            "{\"role\":\"system\",\"content\":\"%s\"},"
            "{\"role\":\"user\",\"content\":\"%s\"}"
          "],"
          "\"temperature\":%.1f,"
          "\"max_tokens\":%d"
        "}",
        model, esc_system, esc_user, temperature, max_tokens);

    free(esc_system);
    free(esc_user);

    /* Check if truncated */
    if (written >= (int)output_size) {
        fprintf(stderr, "Error: JSON request truncated\n");
        return -1;
    }

    return 0;
}

/**
 * Parse content from OpenAI response
 *
 * OpenAI responses look like:
 * {"choices":[{"message":{"content":"COMMAND_HERE"}}],...}
 *
 * We use a simple pattern-matching approach:
 * 1. Find "choices":[
 * 2. Find "message":{
 * 3. Find "content":"
 * 4. Extract string until closing "
 *
 * This works because:
 * - We control the request format (no nested arrays)
 * - OpenAI responses are consistent
 * - We only care about the first choice
 *
 * A real JSON parser would:
 * - Build an AST (abstract syntax tree)
 * - Navigate with proper structure
 * - Handle any valid JSON
 *
 * But we don't need that complexity here!
 */
int json_parse_content(const char *json_response, char *output, size_t output_size) {
    if (!json_response || !output || output_size == 0) {
        return -1;
    }

    /* Pattern: "choices":[{"message":{"content":" */
    const char *choices = strstr(json_response, "\"choices\"");
    if (!choices) {
        fprintf(stderr, "Error: No 'choices' in JSON response\n");
        return -1;
    }

    const char *message = strstr(choices, "\"message\"");
    if (!message) {
        fprintf(stderr, "Error: No 'message' in choices\n");
        return -1;
    }

    const char *content = strstr(message, "\"content\"");
    if (!content) {
        fprintf(stderr, "Error: No 'content' in message\n");
        return -1;
    }

    /* Find the opening quote of the content string */
    const char *value_start = strchr(content + 9, '"'); /* Skip "content" */
    if (!value_start) {
        fprintf(stderr, "Error: Malformed content field\n");
        return -1;
    }
    value_start++; /* Move past the opening quote */

    /* Find the closing quote (handle escaped quotes) */
    const char *value_end = value_start;
    while (*value_end) {
        if (*value_end == '"' && *(value_end - 1) != '\\') {
            break; /* Found unescaped closing quote */
        }
        value_end++;
    }

    if (*value_end != '"') {
        fprintf(stderr, "Error: Unterminated content string\n");
        return -1;
    }

    /* Extract the content */
    size_t content_len = value_end - value_start;
    if (content_len >= output_size) {
        fprintf(stderr, "Warning: Content truncated\n");
        content_len = output_size - 1;
    }

    memcpy(output, value_start, content_len);
    output[content_len] = '\0';

    /*
     * TODO: Unescape the content string
     * (Handle \\n → \n, \\" → ", etc.)
     * For now, most LLMs don't use escapes in command responses
     */

    return 0;
}
