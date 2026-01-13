/**
 * prompt_loader.c - LLM Prompt File Loader Implementation
 *
 * EDUCATIONAL PROJECT - Heavily commented for learning!
 *
 * This file implements loading and formatting of LLM prompts from external
 * text files. See prompt_loader.h for architecture overview.
 *
 * ## Implementation Strategy
 *
 * 1. Read entire prompt files into memory at startup (they're small)
 * 2. Store as null-terminated strings
 * 3. For user prompts, do simple string substitution of placeholders
 * 4. Fall back to hardcoded defaults if files missing
 *
 * ## Memory Management
 *
 * - Prompts are loaded once at startup and kept in static buffers
 * - This is fine because prompts are read-only after loading
 * - Total memory: ~8KB for both prompts (very reasonable)
 * - Freed explicitly in prompt_loader_shutdown()
 *
 * ## Error Handling Philosophy
 *
 * - Missing files → use defaults, warn user, continue
 * - File too large → truncate, warn user, continue
 * - Invalid substitution → skip, warn user, continue
 * - Never fatal - game must always be playable!
 */

#include "prompt_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Maximum size for each prompt (4KB should be plenty) */
#define MAX_PROMPT_SIZE 4096

/* Placeholder strings we'll replace */
#define PLACEHOLDER_CONTEXT "{CONTEXT}"
#define PLACEHOLDER_INPUT "{INPUT}"

/* Storage for loaded prompts */
static char *system_prompt = NULL;
static char *user_template = NULL;

/* Default prompts (fallback if files missing) */
static const char *default_system_prompt =
    "You are a Zork command translator. Translate natural language to Zork commands.\n"
    "Output ONLY the commands, nothing else. Use classic syntax like 'north', 'take lamp'.\n"
    "If multiple commands needed, separate with commas: 'north, open door'.\n";

static const char *default_user_template =
    "Context: {CONTEXT}\nTranslate to Zork commands: {INPUT}";

/**
 * Read an entire file into a newly allocated string
 *
 * This is a utility function that handles all the details of reading a file:
 * 1. Open the file
 * 2. Determine its size
 * 3. Allocate memory
 * 4. Read entire contents
 * 5. Null-terminate
 *
 * @param filepath Path to file
 * @return Newly allocated string with file contents, or NULL on error
 */
static char *read_file_to_string(const char *filepath) {
    /* Open file for reading */
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        fprintf(stderr, "Warning: Could not open %s: %s\n",
                filepath, strerror(errno));
        return NULL;
    }

    /* Allocate buffer for file contents */
    char *buffer = malloc(MAX_PROMPT_SIZE);
    if (!buffer) {
        fprintf(stderr, "Error: Out of memory loading prompt\n");
        fclose(fp);
        return NULL;
    }

    /* Read entire file into buffer */
    size_t bytes_read = fread(buffer, 1, MAX_PROMPT_SIZE - 1, fp);
    fclose(fp);

    /* Null-terminate the string */
    buffer[bytes_read] = '\0';

    /* Warn if file was truncated */
    if (bytes_read == MAX_PROMPT_SIZE - 1) {
        fprintf(stderr, "Warning: Prompt file %s truncated to %d bytes\n",
                filepath, MAX_PROMPT_SIZE);
    }

    return buffer;
}

/**
 * Substitute a placeholder in a string
 *
 * Simple string substitution: replaces {PLACEHOLDER} with value.
 * This is not a full template engine - just enough for our use case.
 *
 * Algorithm:
 * 1. Find first occurrence of placeholder
 * 2. Copy everything before placeholder
 * 3. Copy replacement value
 * 4. Copy everything after placeholder
 * 5. Repeat until no more placeholders (or buffer full)
 *
 * @param template Template string with {PLACEHOLDERS}
 * @param placeholder Placeholder to replace (e.g., "{CONTEXT}")
 * @param value Value to substitute (can be empty string)
 * @param output Buffer for result
 * @param output_size Size of output buffer
 * @return 0 on success, -1 if buffer too small
 */
static int substitute_placeholder(const char *template,
                                   const char *placeholder,
                                   const char *value,
                                   char *output,
                                   size_t output_size) {
    /* Handle NULL value as empty string */
    if (!value) {
        value = "";
    }

    size_t placeholder_len = strlen(placeholder);
    size_t value_len = strlen(value);
    size_t output_pos = 0;
    const char *search_pos = template;

    /* Process template string */
    while (*search_pos && output_pos < output_size - 1) {
        /* Look for placeholder */
        const char *placeholder_pos = strstr(search_pos, placeholder);

        if (!placeholder_pos) {
            /* No more placeholders - copy rest of template */
            size_t remaining = strlen(search_pos);
            size_t available = output_size - output_pos - 1;
            size_t to_copy = (remaining < available) ? remaining : available;

            memcpy(output + output_pos, search_pos, to_copy);
            output_pos += to_copy;
            break;
        }

        /* Copy text before placeholder */
        size_t prefix_len = placeholder_pos - search_pos;
        size_t available = output_size - output_pos - 1;
        size_t to_copy = (prefix_len < available) ? prefix_len : available;

        memcpy(output + output_pos, search_pos, to_copy);
        output_pos += to_copy;

        if (output_pos >= output_size - 1) {
            break; /* Buffer full */
        }

        /* Copy replacement value */
        available = output_size - output_pos - 1;
        to_copy = (value_len < available) ? value_len : available;

        memcpy(output + output_pos, value, to_copy);
        output_pos += to_copy;

        /* Move past placeholder */
        search_pos = placeholder_pos + placeholder_len;
    }

    /* Null-terminate */
    output[output_pos] = '\0';

    /* Check if we ran out of space */
    if (output_pos >= output_size - 1 && *search_pos) {
        fprintf(stderr, "Warning: Prompt truncated during substitution\n");
        return -1;
    }

    return 0;
}

/*
 * Public API Implementation
 */

int prompt_loader_init(const char *prompt_dir) {
    char filepath[256];
    int using_defaults = 0;

    /* Construct path to system.txt */
    snprintf(filepath, sizeof(filepath), "%s/system.txt", prompt_dir);

    /* Try to load system prompt */
    system_prompt = read_file_to_string(filepath);
    if (!system_prompt) {
        fprintf(stderr, "Info: Using default system prompt\n");
        /* Allocate and copy default */
        system_prompt = malloc(strlen(default_system_prompt) + 1);
        if (system_prompt) {
            strcpy(system_prompt, default_system_prompt);
        }
        using_defaults = 1;
    }

    /* Construct path to user_template.txt */
    snprintf(filepath, sizeof(filepath), "%s/user_template.txt", prompt_dir);

    /* Try to load user template */
    user_template = read_file_to_string(filepath);
    if (!user_template) {
        fprintf(stderr, "Info: Using default user template\n");
        /* Allocate and copy default */
        user_template = malloc(strlen(default_user_template) + 1);
        if (user_template) {
            strcpy(user_template, default_user_template);
        }
        using_defaults = 1;
    }

    /* Warn if both are missing */
    if (using_defaults) {
        fprintf(stderr, "\nTip: Create prompt files in '%s/' for better results!\n",
                prompt_dir);
        fprintf(stderr, "See prompts/README.md for documentation.\n\n");
    }

    return using_defaults ? -1 : 0;
}

const char *prompt_loader_get_system_prompt(void) {
    /* Return system prompt (either loaded or default) */
    return system_prompt ? system_prompt : default_system_prompt;
}

int prompt_loader_format_user_prompt(const char *context,
                                      const char *user_input,
                                      char *output,
                                      size_t output_size) {
    if (!user_template || !user_input || !output || output_size == 0) {
        return -1;
    }

    /* Create temporary buffer for intermediate substitution */
    char *temp = malloc(output_size);
    if (!temp) {
        fprintf(stderr, "Error: Out of memory formatting prompt\n");
        return -1;
    }

    /* First, substitute {CONTEXT} */
    if (substitute_placeholder(user_template, PLACEHOLDER_CONTEXT,
                                context ? context : "", temp, output_size) != 0) {
        free(temp);
        return -1;
    }

    /* Then, substitute {INPUT} into the result */
    if (substitute_placeholder(temp, PLACEHOLDER_INPUT,
                                user_input, output, output_size) != 0) {
        free(temp);
        return -1;
    }

    free(temp);
    return 0;
}

void prompt_loader_shutdown(void) {
    /* Free allocated prompts */
    if (system_prompt) {
        free(system_prompt);
        system_prompt = NULL;
    }
    if (user_template) {
        free(user_template);
        user_template = NULL;
    }
}
