# Flexible LLM Architecture for Quietbox 2 Tech Demo

**Created**: February 13, 2026
**Purpose**: Showcase architectural flexibility - switch between 4-model and 1-model modes
**Target**: Quietbox 2 demonstration and educational use

---

## Vision: Choose Your Architecture

This system supports **TWO deployment modes** with **ONE codebase**:

### Mode 1: Multi-Agent (4 Small Models) 🤖🤖🤖🤖
**Use case**: Maximum throughput, fault isolation, multi-agent research

```
User → Router → [ Translator ] [ Artist ] [ DM ] [ Player ]
                  Chip 0       Chip 1    Chip 2  Chip 3
```

**Benefits**:
- 4 parallel requests
- Specialized per task
- Fault isolation
- Uses all hardware
- **Best for learning/demo**

---

### Mode 2: Unified (1 Large Model) 🦾
**Use case**: Best quality, simpler deployment, single-player games

```
User → Router → [ Qwen3-32B (all roles via prompts) ]
                  Chips 0-3 (tensor parallel)
```

**Benefits**:
- Higher quality
- Simpler to manage
- Context sharing
- Lower per-request latency
- **Best for production**

---

## Configuration: Single Switch

**File**: `config/llm_mode.yaml`

```yaml
# LLM Architecture Mode
mode: multi_agent  # or: unified

multi_agent:
  enabled: true
  endpoints:
    translator:
      url: http://localhost:8000/v1/chat/completions
      model: /home/ttuser/models/Qwen3-0.6B
      device_id: 0
      max_tokens: 500
      temperature: 0.1

    artist:
      url: http://localhost:8001/v1/chat/completions
      model: /home/ttuser/models/Qwen3-0.6B
      device_id: 1
      max_tokens: 500
      temperature: 0.8

    dm:
      url: http://localhost:8002/v1/chat/completions
      model: /home/ttuser/models/Qwen3-0.6B
      device_id: 2
      max_tokens: 500
      temperature: 0.9

    player:
      url: http://localhost:8003/v1/chat/completions
      model: /home/ttuser/models/Qwen3-0.6B
      device_id: 3
      max_tokens: 500
      temperature: 0.7

unified:
  enabled: false
  endpoint:
    url: http://localhost:9000/v1/chat/completions
    model: /home/ttuser/models/Qwen3-32B
    tensor_parallel: 4  # Use all 4 chips
    max_tokens: 1000
    temperature: 0.7

  # Role-based prompts embedded in system messages
  role_prompts:
    translator: "prompts/unified/translator_role.txt"
    artist: "prompts/unified/artist_role.txt"
    dm: "prompts/unified/dm_role.txt"
    player: "prompts/unified/player_role.txt"
```

---

## Implementation: LLM Router Abstraction

**File**: `src/llm/llm_router.{h,c}`

### API (mode-agnostic)

```c
/**
 * Initialize LLM router (reads config, determines mode)
 */
int llm_router_init(const char* config_file);

/**
 * Send request (automatically routes based on mode)
 *
 * In multi-agent mode: Routes to specialized endpoint
 * In unified mode: Adds role-specific prompt prefix
 */
int llm_router_request(
    LLMTaskType task,           // TRANSLATE, VISUALIZE, NARRATE, AUTOPLAY
    const char* prompt,          // User input
    char* response_buffer,       // Output buffer
    size_t buffer_size           // Buffer size
);

/**
 * Get current mode
 */
LLMMode llm_router_get_mode(void);  // Returns: MODE_MULTI_AGENT or MODE_UNIFIED

/**
 * Cleanup
 */
void llm_router_shutdown(void);
```

### Implementation Detail

```c
typedef enum {
    MODE_MULTI_AGENT,  // 4 small models
    MODE_UNIFIED       // 1 large model
} LLMMode;

typedef struct {
    LLMMode mode;

    // Multi-agent endpoints
    struct {
        char url[512];
        char model[256];
        int max_tokens;
        float temperature;
    } endpoints[4];

    // Unified endpoint
    struct {
        char url[512];
        char model[256];
        int max_tokens;
        float temperature;
        char role_prompts[4][256];  // Paths to role prompt files
    } unified;

} LLMRouterConfig;

// Internal routing logic
static int route_multi_agent(LLMTaskType task, const char* prompt,
                              char* response, size_t size) {
    // Select endpoint based on task
    // Make direct API call
    // Extract command from <think> tags
}

static int route_unified(LLMTaskType task, const char* prompt,
                        char* response, size_t size) {
    // Load role-specific prompt
    // Combine: role_prompt + user_input
    // Make API call to unified endpoint
    // Parse response based on role
}
```

---

## Startup Scripts: Mode-Aware

### Multi-Agent Mode

**Script**: `scripts/start-multi-agent.sh`

```bash
#!/bin/bash
# Start 4 independent Qwen3-0.6B servers

tt stack use qwen3-0.6b-p100

echo "Starting Multi-Agent Architecture (4x Qwen3-0.6B)"

tt serve start Qwen3-0.6B --device-id 0 --port 8000 --detach
sleep 15
tt serve start Qwen3-0.6B --device-id 1 --port 8001 --detach
sleep 15
tt serve start Qwen3-0.6B --device-id 2 --port 8002 --detach
sleep 15
tt serve start Qwen3-0.6B --device-id 3 --port 8003 --detach

./scripts/check-server-health.sh
```

---

### Unified Mode

**Script**: `scripts/start-unified.sh`

```bash
#!/bin/bash
# Start 1 large model with tensor parallelism

tt stack use qwen3-32b-p150x4  # Or llama-3.3-70b

echo "Starting Unified Architecture (1x Qwen3-32B, 4-chip tensor parallel)"

tt serve start Qwen3-32B \
  --port 9000 \
  --tensor-parallel 4 \
  --max-model-len 4096 \
  --detach

# Wait for startup
sleep 60

curl -s http://localhost:9000/health && echo "✅ Unified server ready!"
```

---

### Auto-Detect Mode

**Script**: `scripts/start-llms.sh` (smart launcher)

```bash
#!/bin/bash
# Automatically start servers based on config

MODE=$(grep "^mode:" config/llm_mode.yaml | awk '{print $2}')

case $MODE in
  multi_agent)
    echo "Config mode: multi_agent"
    ./scripts/start-multi-agent.sh
    ;;
  unified)
    echo "Config mode: unified"
    ./scripts/start-unified.sh
    ;;
  *)
    echo "Invalid mode: $MODE"
    echo "Valid: multi_agent, unified"
    exit 1
    ;;
esac
```

---

## Role-Based Prompts (Unified Mode)

When in unified mode, role-specific context is added to each request:

### Translator Role

**File**: `prompts/unified/translator_role.txt`

```
[ROLE: Command Translator]

You are translating natural language to Zork commands.

Format: Think briefly, then output command.

Examples:
Input: open the mailbox
<think>Remove "the"</think>
open mailbox

Input: pick up lamp
<think>"pick up" = "take"</think>
take lamp

Translate this:
```

### Artist Role

**File**: `prompts/unified/artist_role.txt`

```
[ROLE: ASCII Artist]

Generate ASCII art for Zork locations.

Format: 60 chars wide, 10 lines tall, use ╔═╗║╚╝ borders.

Location: {LOCATION}
Description: {DESCRIPTION}

Generate art:
```

### DM Role

**File**: `prompts/unified/dm_role.txt`

```
[ROLE: Dungeon Master]

Add 1-2 sentences of atmospheric detail BEFORE the game text.
Never change the game text itself.

Standard: {GAME_TEXT}
Recent actions: {ACTIONS}

Enhanced:
```

### Player Role

**File**: `prompts/unified/player_role.txt`

```
[ROLE: AI Player]

Strategic Zork player. Analyze state and suggest next move.

State: {GAME_STATE}

Output JSON:
{
  "command": "...",
  "reasoning": "...",
  "confidence": 0.0-1.0
}
```

---

## Tech Demo Script

**For Quietbox 2 Showcase:**

```bash
# Demo Part 1: Multi-Agent (show all 4 chips working)
echo "=== MULTI-AGENT MODE ==="
echo "Running 4 specialized models, one per chip"

# Edit config
sed -i 's/mode:.*/mode: multi_agent/' config/llm_mode.yaml

# Start servers
./scripts/start-llms.sh

# Show all 4 servers
tt serve status

# Run Zork with natural language
./zork-native game/zork1.z3
> "please open the mailbox and take whatever is inside"
# Show translator on chip 0 converts to commands
# Show ASCII artist on chip 1 generates scene
# Show DM on chip 2 adds flavor

# Demo Part 2: Switch to Unified (show quality improvement)
echo "=== UNIFIED MODE ==="
echo "Switching to single large model (better quality)"

# Stop multi-agent servers
./scripts/stop-four-llms.sh

# Switch config
sed -i 's/mode:.*/mode: unified/' config/llm_mode.yaml

# Start unified server
./scripts/start-llms.sh

# Same gameplay, better quality
./zork-native game/zork1.z3
> "please open the mailbox and take whatever is inside"
# Show smarter translations, better art, richer descriptions

# Demo Part 3: Show it's the same code
echo "=== SAME CODE, DIFFERENT ARCHITECTURE ==="
echo "Router abstraction makes it seamless"
cat src/llm/llm_router.c | grep "llm_router_request"
```

---

## Performance Comparison

| Metric | Multi-Agent (4x0.6B) | Unified (1x32B) |
|--------|---------------------|-----------------|
| **Throughput** | 4 requests/sec | 1 request/sec |
| **Latency (single)** | ~1.5s | ~2-3s |
| **Quality** | Good | Excellent |
| **Memory** | 4× 1.4GB = 5.6GB | 1× 32GB = 32GB |
| **Fault tolerance** | High (isolated) | Low (single point) |
| **Best for** | Parallel workloads | Interactive gaming |
| **Chip utilization** | 100% (4 chips) | 100% (4 chips) |
| **Demo value** | Shows multi-agent | Shows power |

---

## Benefits for Quietbox 2 Demo

### Educational Value ✅
- Shows both architectural patterns
- Demonstrates Tenstorrent flexibility
- Real-world design tradeoffs

### Technical Showcase ✅
- Multi-chip coordination (4-model)
- Tensor parallelism (1-model)
- Hot-swappable architectures
- Production-ready abstraction

### Business Value ✅
- "Configure for your workload"
- Multi-tenant? Use multi-agent
- Single-user? Use unified
- One platform, multiple strategies

---

## Implementation Checklist

### Phase 1: Abstraction Layer
- [ ] Create `src/llm/llm_router.{h,c}`
- [ ] Implement mode detection from config
- [ ] Route to multi-agent endpoints
- [ ] Route to unified endpoint
- [ ] Command extraction utilities

### Phase 2: Configuration
- [ ] Create `config/llm_mode.yaml`
- [ ] Load and parse config in C
- [ ] Validate configuration
- [ ] Handle missing files gracefully

### Phase 3: Unified Mode Support
- [ ] Create role-based prompts
- [ ] Test with Qwen3-32B or Llama-3.3-70B
- [ ] Measure quality improvements
- [ ] Document performance

### Phase 4: Demo Package
- [ ] `scripts/start-llms.sh` (auto-detect)
- [ ] `scripts/start-multi-agent.sh`
- [ ] `scripts/start-unified.sh`
- [ ] `scripts/switch-mode.sh` (interactive)
- [ ] Demo script for presentations

### Phase 5: Documentation
- [ ] User guide: "Choosing Your Mode"
- [ ] Architecture diagrams
- [ ] Performance benchmarks
- [ ] Demo video script

---

## Quick Commands

```bash
# Check current mode
grep "^mode:" config/llm_mode.yaml

# Switch to multi-agent
./scripts/switch-mode.sh multi_agent

# Switch to unified
./scripts/switch-mode.sh unified

# Start (auto-detects mode)
./scripts/start-llms.sh

# Status
tt serve status

# Test current mode
./scripts/test-current-mode.sh
```

---

## Success Metrics

- [ ] Switch modes in < 5 minutes
- [ ] Both modes work with same Zork binary
- [ ] Router adds < 10ms overhead
- [ ] Config changes don't require recompilation
- [ ] Demo impresses Quietbox 2 audience

---

## Future Extensions

### Mode 3: Hybrid
- Critical path (translation) → Unified (quality)
- Background tasks (art, DM) → Multi-agent (parallel)

### Mode 4: Dynamic
- Start with unified
- Scale to multi-agent under load
- Elastic based on request patterns

### Mode 5: Federated
- Different models for different roles
- Mix sizes: 0.6B translator, 32B DM
- Optimize cost/quality per task

---

## Key Insight

**The abstraction layer is the magic.**

By designing `llm_router` properly, we:
- ✅ Support both modes transparently
- ✅ Can add new modes easily
- ✅ Make Zork code mode-agnostic
- ✅ Create a killer tech demo

**Quietbox 2 tagline:**
*"One architecture, infinite configurations. You choose."*

---

**Next Steps:**
1. Implement `llm_router.{h,c}` with mode support
2. Create config file and loader
3. Test both modes thoroughly
4. Prepare demo script
5. Benchmark and document

**This is going to be an amazing demo!** 🚀
