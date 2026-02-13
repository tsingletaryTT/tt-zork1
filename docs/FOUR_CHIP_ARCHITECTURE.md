# Four-Chip Multi-LLM Architecture for Zork

**Created**: February 13, 2026
**Status**: Ready for Implementation
**Branch**: `feature/multi-llm-integration`

## Executive Summary

Run **4 independent Qwen3-0.6B instances** (one per P300C chip), each specialized for a different AI task in an enhanced Zork experience.

## Hardware Configuration

```
System: 4× Tenstorrent P300C (Blackhole)
├── Device 0 (0000:01:00.0) → Port 8000 → Command Translator
├── Device 1 (0000:02:00.0) → Port 8001 → ASCII Artist
├── Device 2 (0000:03:00.0) → Port 8002 → Dungeon Master
└── Device 3 (0000:04:00.0) → Port 8003 → AI Player
```

**Model**: Qwen3-0.6B (1.4 GB per instance = 5.6 GB total)
**Stack**: `qwen3-0.6b-p100` (single chip per instance) ✅ Active

## The Four LLMs

### LLM 1: Command Translator (Device 0, Port 8000)

**Purpose**: Convert natural language to Zork commands (existing Phase 2 functionality)

**Input**:
```
"I want to pick up the lamp and look around"
```

**Output**:
```
take lamp
look
```

**Prompt Template** (`prompts/translator_system.txt`):
```
You are a command translator for the Zork text adventure game.
Convert natural language into valid Zork commands.

Examples:
- "open the mailbox" → "open mailbox"
- "pick up lamp" → "take lamp"
- "go to the north" → "north"

Rules:
1. Use exact Zork command syntax
2. Keep object names from input
3. One command per line if multiple actions
4. Return ONLY the commands, no explanation

Input: {USER_INPUT}
Commands:
```

**Performance Target**: <100ms per translation

---

### LLM 2: ASCII Artist (Device 1, Port 8001)

**Purpose**: Generate ASCII art representations of the current scene

**Input**:
```json
{
  "location": "West of House",
  "description": "You are standing in an open field west of a white house, with a boarded front door.",
  "objects": ["white house", "small mailbox", "forest"],
  "mood": "peaceful"
}
```

**Output**:
```
╔════════════════════════════════════════╗
║         WEST OF HOUSE                  ║
╠════════════════════════════════════════╣
║                                        ║
║        🌲🌲      ___________          ║
║      🌲🌲🌲    |  _______  |          ║
║     🌲🌲🌲🌲   | |       | |          ║
║      🌲🌲🌲     | |   X   | |          ║
║        🌲       | |_______| |          ║
║                 |___________|          ║
║          📬                             ║
║                                        ║
╚════════════════════════════════════════╝
```

**Prompt Template** (`prompts/artist_system.txt`):
```
You are an ASCII artist for the Zork text adventure game.
Create text-based visual representations of game scenes using box-drawing characters and emojis.

Style guidelines:
1. Use ╔═╗║╚╝ for borders
2. Maximum width: 60 characters
3. Maximum height: 12 lines
4. Use emojis sparingly for key objects (🌲 trees, 🏠 house, 📬 mailbox, 🗝️ key, 💡 lamp, etc.)
5. Keep it atmospheric but simple
6. No color codes (terminal compatibility)

Input: {SCENE_JSON}
ASCII Art:
```

**Performance Target**: <500ms per scene
**Caching**: Cache by location name (most locations visited multiple times)

---

### LLM 3: Dungeon Master (Device 2, Port 8002)

**Purpose**: Dynamically enhance room descriptions based on player actions and game state

**Input**:
```json
{
  "location": "West of House",
  "standard_description": "You are standing in an open field west of a white house...",
  "player_actions": ["opened mailbox", "took leaflet"],
  "time_of_day": "afternoon",
  "previous_location": "Forest Path"
}
```

**Output**:
```
As you emerge from the shadowy forest path, afternoon sunlight
washes over you. The white house before you stands silent, its
boarded windows like closed eyes. The mailbox door swings gently
in the breeze, still open from your inspection. You clutch the
weathered leaflet, its edges soft with age.

You are standing in an open field west of a white house, with
a boarded front door. There is a small mailbox here.
```

**Prompt Template** (`prompts/dm_system.txt`):
```
You are a creative Dungeon Master for Zork, enhancing the atmosphere
without changing game logic. Add flavor text BEFORE the standard
description.

Guidelines:
1. Keep flavor text to 2-3 sentences maximum
2. Reference recent player actions if relevant
3. Add atmospheric details (weather, sounds, smells)
4. Never contradict the game state
5. Always include the full standard description at the end
6. Match the tone (mysterious/dangerous/peaceful)

Input: {SCENE_JSON}
Enhanced Description:
```

**Performance Target**: <800ms per enhancement
**Toggle**: Can be disabled via `ZORK_DM_ENABLED=0`

---

### LLM 4: AI Player (Device 3, Port 8003)

**Purpose**: Autonomous agent that can play Zork (observer/co-pilot/autonomous modes)

**Input**:
```json
{
  "current_location": "West of House",
  "visible_objects": ["white house", "mailbox", "forest"],
  "inventory": ["brass lantern", "leaflet"],
  "recent_output": "You are standing in an open field...",
  "goal": "explore and find treasures",
  "mode": "observer"
}
```

**Output**:
```json
{
  "suggested_command": "go north",
  "reasoning": "The house appears inaccessible from here (boarded door). North is unexplored and likely leads to new areas. Standard opening exploration pattern.",
  "confidence": 0.85,
  "alternatives": ["enter house", "open mailbox"],
  "strategy_notes": "Following exploration strategy: exhaust obvious paths before attempting puzzles."
}
```

**Prompt Template** (`prompts/player_system.txt`):
```
You are an AI player for Zork. Your goal is to explore, solve puzzles,
and collect treasures. Think step-by-step.

Current state: {STATE_JSON}

Strategies:
1. EXPLORE: Visit all accessible locations systematically
2. COLLECT: Take all portable items
3. PUZZLE: Try item combinations when stuck
4. MAP: Build mental model of the game world
5. SURVIVE: Avoid death (watch for Grues in darkness!)

Provide:
- suggested_command: Single best action
- reasoning: Why this action makes sense
- confidence: 0.0 to 1.0
- alternatives: 2-3 backup options

Response (JSON):
```

**Performance Target**: <1000ms per decision
**Modes**:
- **Observer**: Suggests moves, human decides
- **Co-pilot**: AI plays, human can interrupt
- **Autonomous**: Fully automatic gameplay
- **Teaching**: Explains reasoning for educational purposes

---

## Implementation Plan

### Phase 1: Multi-Server Infrastructure ✅ Ready

**Goal**: Start 4 independent vLLM servers, one per chip

**Commands**:
```bash
# Ensure using single-chip stack
tt stack use qwen3-0.6b-p100

# Start 4 independent servers (one per chip)
tt serve start Qwen3-0.6B --device-id 0 --port 8000 --detach  # Translator
tt serve start Qwen3-0.6B --device-id 1 --port 8001 --detach  # Artist
tt serve start Qwen3-0.6B --device-id 2 --port 8002 --detach  # DM
tt serve start Qwen3-0.6B --device-id 3 --port 8003 --detach  # Player

# Verify all running
tt serve status
```

**Expected Output**:
```
Running Servers:

  Qwen3-0.6B (Device 0)
    PID: 10001
    Port: 8000
    Device: 0

  Qwen3-0.6B (Device 1)
    PID: 10002
    Port: 8001
    Device: 1

  Qwen3-0.6B (Device 2)
    PID: 10003
    Port: 8002
    Device: 2

  Qwen3-0.6B (Device 3)
    PID: 10004
    Port: 8003
    Device: 3
```

**Startup Script** (`scripts/start-four-llms.sh`):
```bash
#!/bin/bash
# Start 4-LLM Zork Architecture

set -e

echo "🚀 Starting 4-LLM Zork Architecture..."

# Ensure correct stack
tt stack use qwen3-0.6b-p100

# Start servers
echo "Starting Command Translator (Device 0, Port 8000)..."
tt serve start Qwen3-0.6B --device-id 0 --port 8000 --detach

echo "Starting ASCII Artist (Device 1, Port 8001)..."
tt serve start Qwen3-0.6B --device-id 1 --port 8001 --detach

echo "Starting Dungeon Master (Device 2, Port 8002)..."
tt serve start Qwen3-0.6B --device-id 2 --port 8002 --detach

echo "Starting AI Player (Device 3, Port 8003)..."
tt serve start Qwen3-0.6B --device-id 3 --port 8003 --detach

# Wait for all servers to be ready
echo ""
echo "Waiting for servers to initialize (this may take 2-3 minutes)..."
for port in 8000 8001 8002 8003; do
  while ! curl -s http://localhost:$port/health > /dev/null 2>&1; do
    echo -n "."
    sleep 5
  done
  echo " ✓ Port $port ready"
done

echo ""
echo "✅ All servers ready!"
echo ""
tt serve status
```

**Shutdown Script** (`scripts/stop-four-llms.sh`):
```bash
#!/bin/bash
# Stop all 4 LLM servers

echo "Stopping all Qwen3-0.6B servers..."

for device in 0 1 2 3; do
  tt stop Qwen3-0.6B --device-id $device
done

echo "✅ All servers stopped"
```

---

### Phase 2: LLM Router Module

**Goal**: Central dispatcher for routing requests to specialized endpoints

**New Module**: `src/llm/llm_router.{h,c}`

```c
/**
 * llm_router.h - Multi-LLM Request Router
 *
 * Routes different AI tasks to specialized LLM endpoints.
 * Each endpoint runs on a separate Tenstorrent chip for maximum throughput.
 */

#ifndef LLM_ROUTER_H
#define LLM_ROUTER_H

typedef enum {
    LLM_TASK_TRANSLATE,      // Command translation (Device 0)
    LLM_TASK_VISUALIZE,      // ASCII art generation (Device 1)
    LLM_TASK_NARRATE,        // Story enhancement (Device 2)
    LLM_TASK_AUTOPLAY        // AI player (Device 3)
} LLMTaskType;

typedef struct {
    char url[512];           // API endpoint
    char model[128];         // Model name
    LLMTaskType task;        // Task specialization
    int enabled;             // Runtime toggle
    int device_id;           // Chip number
    int port;                // Server port
} LLMEndpoint;

/**
 * Initialize the LLM router with 4 endpoints
 * Returns 0 on success, -1 on failure
 */
int llm_router_init(void);

/**
 * Send a request to the appropriate LLM based on task type
 *
 * @param task - Which LLM to use
 * @param prompt - Input prompt
 * @param response_buffer - Buffer for response (caller allocated)
 * @param buffer_size - Size of response buffer
 * @return 0 on success, -1 on failure
 */
int llm_router_request(LLMTaskType task, const char* prompt,
                       char* response_buffer, size_t buffer_size);

/**
 * Check if a specific LLM endpoint is available
 */
int llm_router_is_available(LLMTaskType task);

/**
 * Get statistics for an endpoint (requests, failures, avg latency)
 */
void llm_router_get_stats(LLMTaskType task,
                          int* requests, int* failures, float* avg_ms);

/**
 * Shutdown router and cleanup
 */
void llm_router_shutdown(void);

#endif /* LLM_ROUTER_H */
```

**Configuration** (environment variables):
```bash
# Translator (existing)
export ZORK_LLM_TRANSLATE_URL="http://localhost:8000/v1/chat/completions"
export ZORK_LLM_TRANSLATE_ENABLED=1

# ASCII Artist
export ZORK_LLM_VISUALIZE_URL="http://localhost:8001/v1/chat/completions"
export ZORK_LLM_VISUALIZE_ENABLED=1

# Dungeon Master
export ZORK_LLM_NARRATE_URL="http://localhost:8002/v1/chat/completions"
export ZORK_LLM_NARRATE_ENABLED=1

# AI Player
export ZORK_LLM_AUTOPLAY_URL="http://localhost:8003/v1/chat/completions"
export ZORK_LLM_AUTOPLAY_ENABLED=0  # Off by default
export ZORK_LLM_AUTOPLAY_MODE="observer"  # observer/copilot/autonomous
```

---

### Phase 3: ASCII Artist Integration

**New Module**: `src/llm/scene_visualizer.{h,c}`

```c
/**
 * scene_visualizer.h - ASCII Art Scene Generator
 *
 * Generates text-based visual representations of Zork locations
 * using LLM on Device 1 (Port 8001).
 */

/**
 * Generate ASCII art for current location
 *
 * @param location - Location name (e.g., "West of House")
 * @param description - Standard game description
 * @param objects - Visible objects (array)
 * @param num_objects - Number of objects
 * @param art_buffer - Buffer for ASCII art output
 * @param buffer_size - Size of buffer
 * @return 0 on success, -1 on failure
 */
int visualizer_generate_scene(const char* location,
                               const char* description,
                               const char** objects,
                               int num_objects,
                               char* art_buffer,
                               size_t buffer_size);

/**
 * Clear visualization cache (force regeneration)
 */
void visualizer_clear_cache(void);
```

**Integration Point**: Hook into `os_display_string()` to detect location changes

**Display Modes**:
1. **Sidebar**: ASCII art to the right of text (wide terminals)
2. **Above**: ASCII art above description (standard terminals)
3. **On-demand**: Show art via special command (`visualize` or `look art`)

---

### Phase 4: Dungeon Master Integration

**New Module**: `src/llm/dungeon_master.{h,c}`

```c
/**
 * dungeon_master.h - Dynamic Story Enhancement
 *
 * Adds atmospheric flavor text to standard game responses
 * using LLM on Device 2 (Port 8002).
 */

/**
 * Enhance a game response with atmospheric details
 *
 * @param location - Current location
 * @param standard_output - Original game text
 * @param recent_actions - Array of recent player commands
 * @param num_actions - Number of recent actions
 * @param enhanced_buffer - Buffer for enhanced output
 * @param buffer_size - Size of buffer
 * @return 0 on success, -1 on failure
 */
int dm_enhance_output(const char* location,
                      const char* standard_output,
                      const char** recent_actions,
                      int num_actions,
                      char* enhanced_buffer,
                      size_t buffer_size);

/**
 * Set enhancement intensity (0.0 = off, 1.0 = maximum)
 */
void dm_set_intensity(float intensity);
```

**Integration Point**: Intercept `os_display_string()` before displaying to user

**Safety Checks**:
- Never change game state
- Always include standard text
- Fall back to standard on errors
- Respect MAX_ENHANCEMENT_LENGTH

---

### Phase 5: AI Player Integration

**New Module**: `src/llm/ai_player.{h,c}`

```c
/**
 * ai_player.h - Autonomous Zork Player
 *
 * AI agent that plays Zork using LLM on Device 3 (Port 8003).
 */

typedef enum {
    AI_MODE_OBSERVER,      // Suggest moves, human decides
    AI_MODE_COPILOT,       // AI plays, human can interrupt
    AI_MODE_AUTONOMOUS,    // Fully automatic
    AI_MODE_TEACHING       // Explains reasoning
} AIPlayerMode;

/**
 * Initialize AI player
 */
int ai_player_init(AIPlayerMode mode);

/**
 * Get AI's suggested next command
 *
 * @param game_state - Current game state (JSON)
 * @param command_buffer - Buffer for suggested command
 * @param reasoning_buffer - Buffer for explanation
 * @param buffer_sizes - Sizes of buffers
 * @return Confidence score (0.0-1.0), -1.0 on error
 */
float ai_player_suggest_move(const char* game_state,
                              char* command_buffer,
                              char* reasoning_buffer,
                              size_t buffer_size);

/**
 * Execute one AI turn (in copilot/autonomous modes)
 * Returns the command that was executed
 */
const char* ai_player_execute_turn(void);
```

**Integration Point**: New game loop mode in `main.c`

**UI Display** (Observer Mode):
```
West of House
You are standing in an open field west of a white house...

🤖 AI suggests: go north
   Reasoning: Unexplored direction. Standard exploration strategy.
   Confidence: 85%

> _
```

---

## Performance Optimization

### Parallel Requests

When multiple LLMs can run concurrently:

```c
// Example: Generate art while translating command
typedef struct {
    LLMTaskType task;
    char response[MAX_RESPONSE];
    int complete;
    int success;
} LLMRequest;

// Launch requests in parallel
LLMRequest requests[2];
requests[0].task = LLM_TASK_TRANSLATE;
requests[1].task = LLM_TASK_VISUALIZE;

// Both execute on different chips simultaneously!
pthread_create(&thread1, NULL, llm_request_thread, &requests[0]);
pthread_create(&thread2, NULL, llm_request_thread, &requests[1]);

// Wait for completion
pthread_join(thread1, NULL);
pthread_join(thread2, NULL);
```

**Benefit**: Zero added latency for parallel operations!

### Caching Strategies

**ASCII Art**: Cache by location (most visited repeatedly)
```c
typedef struct {
    char location[128];
    char ascii_art[2048];
    time_t generated;
} ArtCacheEntry;
```

**Translations**: Cache common phrases
```c
typedef struct {
    char input[256];
    char commands[128];
    int use_count;
} TranslationCache;
```

**DM Enhancements**: Cache by location + action combination

---

## Resource Requirements

| Component | Memory | Latency | Chip |
|-----------|--------|---------|------|
| Translator | 1.4 GB | ~100ms | 0 |
| Artist | 1.4 GB | ~500ms | 1 |
| DM | 1.4 GB | ~800ms | 2 |
| AI Player | 1.4 GB | ~1000ms | 3 |
| **Total** | **5.6 GB** | **Parallel** | **4** |

**Total System Memory**: ~8 GB (includes vLLM overhead)
**Available on 4× P300C**: Plenty of headroom ✅

---

## Testing Strategy

### Unit Tests

```bash
# Test each LLM independently
./test-llm-translator.sh   # Port 8000
./test-llm-artist.sh       # Port 8001
./test-llm-dm.sh           # Port 8002
./test-llm-player.sh       # Port 8003
```

### Integration Tests

```bash
# Test full game with all LLMs active
./test-four-llm-gameplay.sh

# Test with selective disabling
ZORK_LLM_VISUALIZE_ENABLED=0 ./zork-native
```

### Load Tests

```bash
# Send 100 concurrent requests across all 4 LLMs
./load-test-four-llms.sh
```

---

## Launch Scripts

### Quick Start

```bash
# Start all 4 LLMs and launch game
./scripts/play-zork-four-llms.sh
```

### Advanced

```bash
# Start servers
./scripts/start-four-llms.sh

# Wait for ready
# ... servers initialize ...

# Set configuration
export ZORK_LLM_TRANSLATE_URL="http://localhost:8000/v1/chat/completions"
export ZORK_LLM_VISUALIZE_URL="http://localhost:8001/v1/chat/completions"
export ZORK_LLM_NARRATE_URL="http://localhost:8002/v1/chat/completions"
export ZORK_LLM_AUTOPLAY_URL="http://localhost:8003/v1/chat/completions"

export ZORK_LLM_TRANSLATE_ENABLED=1
export ZORK_LLM_VISUALIZE_ENABLED=1
export ZORK_LLM_NARRATE_ENABLED=1
export ZORK_LLM_AUTOPLAY_ENABLED=0  # Enable manually if desired

# Run game
./zork-native game/zork1.z3

# When done
./scripts/stop-four-llms.sh
```

---

## Future Enhancements

### Phase 6: Image Generation

Replace ASCII Artist with actual image generation:
- Use Stable Diffusion on Tensix cores
- Generate PNG images for each location
- Display via terminal image protocols (iTerm2, Kitty)

### Phase 7: Voice Interface

- Speech-to-text for input (Whisper model)
- Text-to-speech for output (Piper/Coqui)
- Full voice-controlled gameplay

### Phase 8: Multi-Player

- Run multiple game instances
- AI players compete/cooperate
- Leaderboard system

### Phase 9: LLM-to-LLM Gameplay

- 4 AI players, each on separate chip
- Tournament mode
- Emergent strategies

---

## Troubleshooting

### Server Won't Start

```bash
# Check if port is already in use
lsof -i :8000

# Check device availability
tt device info

# Check stack configuration
tt stack list
```

### High Latency

```bash
# Check if servers are healthy
tt serve status

# Test endpoint directly
curl -X POST http://localhost:8000/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{"model":"Qwen3-0.6B","messages":[{"role":"user","content":"test"}]}'
```

### Out of Memory

```bash
# Reduce max_num_seqs or max_model_len
tt serve start Qwen3-0.6B --device-id 0 --port 8000 \
  --max-num-seqs 8 --max-model-len 1024 --detach
```

---

## Success Metrics

- ✅ All 4 servers start successfully
- ✅ Each server isolated to its chip (verified via device ID)
- ✅ Translation latency < 200ms
- ✅ ASCII art generation < 1s
- ✅ DM enhancement < 1.5s
- ✅ AI player decision < 2s
- ✅ Parallel requests work correctly
- ✅ Game remains playable with all features enabled
- ✅ Graceful degradation if any LLM fails

---

## References

- tt-cli Cookbook: `/home/ttuser/code/tt-cli/COOKBOOK.md`
- vLLM Documentation: `/home/ttuser/tt-vllm/`
- Phase 2 LLM Integration: `docs/CONTEXT_FREE_MODE.md`
- Original Architecture: `docs/MULTI_LLM_ARCHITECTURE.md`

---

**Next Steps**:
1. Create startup scripts
2. Test multi-server deployment
3. Implement LLM router
4. Add ASCII artist module
5. Integrate DM enhancement
6. Build AI player system

**Let's build the future of interactive fiction - one chip at a time!** 🎮🤖✨
