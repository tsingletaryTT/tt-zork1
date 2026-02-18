# Bugs Found During Testing (2026-02-17)

## Critical Issues

### Issue 1: Slash Commands Echo to Game ❌

**Problem:**
```bash
> /status
=== Current Settings ===
Game Mode: ENHANCED
...
I beg your pardon?    # <-- Game also processes the slash command!
```

**Root Cause:**
In `dinput.c` line 558, `dumb_display_user_input(read_line_buffer)` is called BEFORE checking if input is a slash command (line 567). So the slash command text gets displayed and sent to the Z-machine even though we later intercept it.

**Fix:**
Move slash command processing BEFORE `dumb_display_user_input()`.

```c
/* BEFORE (line 557-581) */
dumb_display_user_input(read_line_buffer);  // Shows command first

slash_result_t slash_result = slash_commands_process(read_line_buffer);
if (slash_result.should_skip) {
    return terminator;  // Too late - already displayed!
}

/* AFTER (correct order) */
slash_result_t slash_result = slash_commands_process(read_line_buffer);
if (slash_result.handled && slash_result.should_skip) {
    /* Don't display slash commands - they're meta-commands */
    buf[0] = '\0';
    read_line_buffer[0] = '\0';
    return terminator;
}

/* Only display non-slash commands */
dumb_display_user_input(read_line_buffer);
```

---

### Issue 2: LLM Client Reinitializes on Every Request ❌

**Problem:**
```
>
LLM client: Initialized    # <-- Prints every single time!
  URL: http://localhost:8000/v1/chat/completions
  Model: Qwen3-0.6B
```

**Root Cause:**
In `llm_router.c` lines 549-554, the router calls:
```c
llm_client_shutdown();
if (llm_client_init() != 0) { ... }
```
on EVERY request to switch between endpoint URLs via environment variables. This is:
- Very inefficient (full reinit every request)
- Causes verbose logging spam
- Potentially causes connection issues

**Fix Options:**

**Option A:** Cache client instances per endpoint (proper fix)
```c
static LLMClient g_clients[4];  // One per task type

// Initialize all clients once
for (int i = 0; i < 4; i++) {
    if (endpoints[i].configured) {
        llm_client_create(&g_clients[i], endpoints[i].url, endpoints[i].model);
    }
}

// Use appropriate client per request
llm_client_request(&g_clients[task], prompt, response);
```

**Option B:** Quick fix - suppress init messages
```c
// In llm_client.c, add a flag to skip printing
static int g_suppress_init_message = 0;

void llm_client_set_quiet(int quiet) {
    g_suppress_init_message = quiet;
}

// Only print on first init
if (!g_suppress_init_message) {
    fprintf(stderr, "LLM client: Initialized\n");
}
```

---

### Issue 3: Auto-Player Fails to Generate Commands ❌

**Problem:**
```
[AI Player → ]
Auto player: Failed to generate command

[Auto-play error: Falling back to manual input]
```

**Root Cause:**
The auto-player calls `llm_router_request(LLM_TASK_AUTOPLAY, ...)` which:
1. Tries to get endpoint config for task index 3 (AUTOPLAY)
2. Finds `endpoints[3]` is configured (player endpoint)
3. But the player endpoint might not be loading correctly
4. OR the persona prompt file path is wrong
5. OR the LLM request is failing for another reason

**Debug Steps:**
1. Check if `endpoints[3]` is actually configured
2. Verify persona prompt files load correctly
3. Check actual LLM API call error messages
4. Verify port 8003 is responding

**Potential Issues:**
- Persona prompts are NOT in the config file (they're loaded separately by auto_player.c)
- Config expects `prompts/player/system_v3_magic.txt` but persona system loads different files
- Need to integrate persona prompt loading with router system

---

### Issue 4: Auto-Player Uses Generic Prompt, Not Personas ❌

**Problem:**
Auto-player loads persona-specific prompts in `auto_player.c` but the router is configured to use `prompts/player/system_v3_magic.txt` from the config file.

**Root Cause:**
Two separate prompt systems:
1. **Router system:** Loads `config/llm_mode.yaml` → `prompts/player/system_v3_magic.txt`
2. **Auto-player system:** Loads persona files → `prompts/player/naive_explorer.txt`

They're not communicating!

**Fix:**
The auto-player needs to:
1. Build the full prompt (system + user) itself
2. Pass it to `llm_client` directly, NOT through the router
3. OR update the router's cached prompt dynamically when persona changes

**Better Approach:**
```c
// In auto_player.c auto_player_next_command()
char full_prompt[MAX_PROMPT_SIZE];

// Load persona-specific prompt
load_prompt_file(get_strategy_prompt_file(g_strategy), persona_prompt, ...);

// Build complete prompt
snprintf(full_prompt, sizeof(full_prompt),
         "%s\n\nCurrent game state:\n%s\n",
         persona_prompt, game_state);

// Call LLM directly, bypass router
llm_client_request_direct(PLAYER_URL, PLAYER_MODEL, full_prompt, output, ...);
```

---

## Minor Issues

### Issue 5: No ASCII Art Visible ⚠️

**Problem:** ASCII art should appear after location descriptions but doesn't.

**Possible Causes:**
1. Artist endpoint (port 8001) is working but output isn't being displayed
2. Output capture might not be feeding to artist agent
3. Artist response parsing might be failing
4. Timing issue (art generated but not shown yet)

**Debug:** Need to check if artist agent is actually being called and what it returns.

---

### Issue 6: Output Capture Not Feeding to Auto-Player ⚠️

**Problem:** Auto-player receives placeholder text "You are playing Zork" instead of actual game output.

**Root Cause:** Lines 594-595 in `dinput.c`:
```c
/* TODO: Integrate with output_capture to provide full game context */
const char *game_state = "You are playing Zork. What do you want to do?";
```

**Fix:** Actually integrate with output_capture:
```c
#include "../../../../llm/output_capture.h"

const char *game_state = output_capture_get_text();
if (!game_state || strlen(game_state) == 0) {
    game_state = "You are playing Zork. What do you want to do?";
}
```

---

## Test Results Summary

### ✅ Working Features
- Natural language translation (Chip 0) - Confirmed working!
- Journey tracking
- Slash command parsing
- Persona prompt file loading

### ❌ Broken Features
- Slash commands echo to game
- Auto-play mode (fails to generate commands)
- ASCII art not visible
- Persona switching (prompts not used correctly)

### 🔧 Needs Improvement
- LLM client reinitializes on every request (performance)
- Auto-player uses placeholder game state (no context)
- Verbose logging spam

---

## Priority Fixes

### High Priority
1. **Fix slash command echo** (breaks user experience)
2. **Fix auto-player command generation** (core feature broken)
3. **Integrate persona prompts with router** (personas don't work)

### Medium Priority
4. Fix LLM client reinitialization (performance)
5. Integrate output_capture with auto-player (context)
6. Debug ASCII art visibility

### Low Priority
7. Suppress verbose logging
8. Optimize prompt caching

---

## Testing Checklist

After fixes, verify:
- [ ] Slash commands don't echo to game
- [ ] `/play auto` + `/player naive` generates AI commands
- [ ] Different personas use different prompts
- [ ] Natural language translation still works
- [ ] ASCII art appears after locations
- [ ] No "LLM client: Initialized" spam
- [ ] Auto-player receives actual game output

---

**Created:** 2026-02-17
**Tester:** Claude (AI Assistant)
**Game Version:** zork-native (commit 82f8810)
