# Multi-LLM Architecture for Zork on Tenstorrent

**Created**: February 9, 2026
**Status**: Planning Phase
**Branch**: `feature/multi-llm-integration`

## Vision

Transform Zork from a simple text adventure into a multi-agent AI experience by running **one LLM per Tenstorrent chip**, each handling a specialized task:

1. **LLM 1: Input Translator** (existing) - Convert natural language to Zork commands
2. **LLM 2: Scene Visualizer** (new) - Generate image descriptions/prompts based on current location
3. **LLM 3: Dungeon Master** (new) - Adapt and enhance the story dynamically
4. **LLM 4: AI Player** (new) - Autonomous agent that plays the game itself

## Hardware Architecture

**System**: Tenstorrent Blackhole (4 chips detected)
- Chip 0: P300C (harvesting mask 0x101)
- Chip 1: P300C (harvesting mask 0x1100) - Currently used for RISC-V work
- Chip 2: P300C
- Chip 3: P300C

**Inference Strategy**: Run vLLM server with tensor parallelism across chips
- Each chip can run its own model instance
- Or use tensor parallelism for larger models split across chips

## Available Models

From `tt model list`:
- **Qwen3-0.6B** (1.4 GB) - Ultra-fast, good for translation
- **Qwen/Qwen3-8B** (30.5 GB) - Balanced quality/speed
- **Llama-3.1-8B-Instruct** (59.9 GB) - Current active stack
- **Qwen/Qwen3-32B** (122.1 GB) - High quality, needs tensor parallelism

## Phase 1: Single vLLM Server (Current Task)

### Goal
Replace Ollama with native Tenstorrent inference using `tt serve`.

### Implementation Steps

1. **Start vLLM Server**
   ```bash
   # Use lightweight model for testing (Qwen3-0.6B)
   tt serve start Qwen3-0.6B --port 8000 --detach

   # Or use active stack (Llama-3.1-8B)
   tt serve start --port 8000 --detach
   ```

2. **Update LLM Client Configuration**
   - Change `DEFAULT_API_URL` from `http://localhost:1234` to `http://localhost:8000`
   - Update `DEFAULT_MODEL` to match vLLM server model
   - Test with existing input translation functionality

3. **Create Launch Script**
   - `run-zork-tt-llm.sh` - Start server + run Zork
   - Handle server lifecycle (start/stop/restart)
   - Check server health before launching game

### Success Criteria
- ✅ vLLM server starts successfully
- ✅ Input translation works (Phase 2 functionality preserved)
- ✅ Performance acceptable (sub-second inference for short prompts)
- ✅ Server runs stably during extended gameplay

## Phase 2: Multi-Endpoint Support

### Goal
Enable the game to communicate with multiple LLM endpoints simultaneously.

### Architecture Changes

**New Module**: `src/llm/llm_router.{h,c}`
- Manages multiple LLM endpoints
- Routes requests based on task type
- Handles concurrent requests (async/parallel)

**Endpoint Configuration**:
```c
typedef enum {
    LLM_TASK_TRANSLATE,      // Input translation
    LLM_TASK_VISUALIZE,      // Scene visualization
    LLM_TASK_NARRATE,        // Story adaptation
    LLM_TASK_AUTOPLAY        // AI player
} LLMTaskType;

typedef struct {
    char url[512];           // API endpoint
    char model[128];         // Model name
    LLMTaskType task;        // Task specialization
    int enabled;             // Runtime toggle
} LLMEndpoint;
```

**Environment Variables**:
```bash
# Existing (Phase 2)
ZORK_LLM_URL=http://localhost:8000/v1/chat/completions
ZORK_LLM_MODEL=Qwen3-0.6B

# New (Phase 2 extension)
ZORK_LLM_VISUALIZER_URL=http://localhost:8001/v1/chat/completions
ZORK_LLM_VISUALIZER_MODEL=Qwen3-8B
ZORK_LLM_DM_URL=http://localhost:8002/v1/chat/completions
ZORK_LLM_DM_MODEL=Qwen3-8B
ZORK_LLM_PLAYER_URL=http://localhost:8003/v1/chat/completions
ZORK_LLM_PLAYER_MODEL=Qwen3-0.6B
```

### Implementation Steps

1. Create LLM router module
2. Add endpoint registration system
3. Implement async request handling (non-blocking)
4. Update existing input translator to use router

## Phase 3: Scene Visualizer

### Goal
Generate visual descriptions of the current game scene for image generation.

### New Module: `src/llm/scene_visualizer.{h,c}`

**Input**: Current location + visible objects + recent events
**Output**: Rich visual description suitable for image generation

**Example**:
```
Location: West of House
Objects: white house, small mailbox, forest
Recent: opened mailbox, took leaflet

→ Visualizer Output:
"Fantasy landscape painting: A weathered white colonial house with
boarded windows sits in a forest clearing. In the foreground, a small
brass mailbox on a wooden post stands open, its door hanging ajar.
Golden afternoon light filters through dense trees in the background.
Style: classic text adventure game cover art, 1980s fantasy illustration."
```

### Integration Points

- **Trigger**: Location change or significant action
- **Display**: Optional sidebar or full-screen mode
- **Caching**: Cache descriptions by location (avoid redundant generation)

### Future Extension
Connect to image generation model (Stable Diffusion, FLUX, etc.) running on Tenstorrent!

## Phase 4: Dungeon Master

### Goal
Dynamically adapt the story based on player actions.

### New Module: `src/llm/dungeon_master.{h,c}`

**Capabilities**:
- Add flavor text to standard responses
- Create new items or obstacles
- Modify puzzle difficulty
- Generate random encounters
- Provide hints when stuck

**Example Interactions**:
```
Standard Zork:
> open mailbox
Opening the small mailbox reveals a leaflet.

With Dungeon Master:
> open mailbox
As you reach for the rusty latch, a gentle breeze stirs the trees.
The mailbox creaks open, revealing a weathered leaflet that seems
to glow faintly in the dappled sunlight. Opening the small mailbox
reveals a leaflet.
```

**Safety Constraints**:
- Never contradicts core game state (Z-machine is authoritative)
- Only adds flavor, doesn't change game logic
- Can be toggled on/off
- Respects difficulty settings (minimal/normal/verbose)

### Implementation Approach

**Interception Pattern**:
```
1. User command → Z-machine
2. Z-machine generates response
3. DM intercepts response
4. DM adds flavor/context
5. Enhanced response displayed to user
```

## Phase 5: AI Player

### Goal
Create an autonomous agent that plays Zork itself.

### New Module: `src/llm/ai_player.{h,c}`

**Capabilities**:
- Read game state and output
- Formulate strategy and goals
- Generate commands to execute
- Learn from failures
- Complete puzzles autonomously

**Modes**:
1. **Observer**: AI suggests moves, human decides
2. **Co-pilot**: AI plays, human can interrupt
3. **Autonomous**: AI plays completely independently
4. **Teaching**: AI explains reasoning for educational purposes

**Example Session**:
```
[AI Player activated - Observer Mode]

Game: West of House. You are standing in an open field west of a
      white house, with a boarded front door. There is a small mailbox
      here.

AI: > open mailbox
Reasoning: Standard opening move - check mailbox for starting items.

Game: Opening the small mailbox reveals a leaflet.

AI: > take leaflet
Reasoning: Collecting items is essential. The leaflet likely contains
           game instructions or clues.

Game: Taken.

AI: > read leaflet
Reasoning: Reading items often provides important information.
```

### Architecture

**State Tracking**:
- Visited locations (map building)
- Inventory management
- Puzzle state (locked/unlocked)
- Failed attempts (learn from mistakes)

**Strategy Components**:
- Exploration (systematic dungeon mapping)
- Item collection (hoarding mode!)
- Puzzle solving (try combinations)
- Goal seeking (treasures, winning conditions)

**LLM Integration**:
- Feed game state as context
- Request next action(s)
- Parse and validate commands
- Update strategy based on results

## Phase 6: Performance Optimization

### Goal
Minimize latency and maximize throughput for 4 concurrent LLMs.

### Strategies

1. **Caching**
   - Cache location descriptions (visualizer)
   - Cache common translations (input translator)
   - Share context between agents

2. **Batching**
   - Combine multiple requests when possible
   - Use vLLM's batching capabilities

3. **Model Selection**
   - Use smaller models for simple tasks (0.6B for translation)
   - Reserve larger models for complex reasoning (8B for DM)

4. **Tensor Parallelism**
   - Split large models across chips
   - Balance load across available hardware

5. **Async Execution**
   - Non-blocking inference calls
   - Update UI as results arrive
   - Graceful degradation if endpoints slow/unavailable

## Development Roadmap

### Sprint 1: Infrastructure (Current)
- [x] Create architecture document
- [ ] Start vLLM server with `tt serve`
- [ ] Update LLM client to use vLLM endpoint
- [ ] Test existing input translation with vLLM
- [ ] Create launch script for integrated setup

### Sprint 2: Multi-Endpoint Foundation
- [ ] Implement LLM router module
- [ ] Add endpoint registration and health checks
- [ ] Create configuration system for multiple endpoints
- [ ] Test concurrent requests

### Sprint 3: Scene Visualizer
- [ ] Implement scene description generation
- [ ] Add location-based triggers
- [ ] Create caching system
- [ ] Integrate with game display

### Sprint 4: Dungeon Master
- [ ] Implement response interception
- [ ] Create flavor text generation
- [ ] Add safety constraints
- [ ] Create difficulty settings

### Sprint 5: AI Player
- [ ] Implement state tracking
- [ ] Create strategy engine
- [ ] Add observer/co-pilot/autonomous modes
- [ ] Build educational explanations

### Sprint 6: Polish & Optimization
- [ ] Performance tuning
- [ ] Comprehensive testing
- [ ] Documentation
- [ ] Demo creation

## Technical Considerations

### vLLM Server Management

**Starting Multiple Servers**:
```bash
# Option 1: Multiple ports, different models
tt serve start Qwen3-0.6B --port 8000 --detach    # Translator
tt serve start Qwen3-8B --port 8001 --detach      # Visualizer
tt serve start Qwen3-8B --port 8002 --detach      # DM
tt serve start Qwen3-0.6B --port 8003 --detach    # AI Player

# Option 2: Single server with model routing (if supported)
tt serve start Qwen3-8B --port 8000 --detach
```

**Resource Considerations**:
- 4 chips available
- Memory constraints per chip
- Tensor parallelism for larger models
- CPU/GPU fallback for development

### API Compatibility

**vLLM API Endpoints** (OpenAI-compatible):
- `/v1/chat/completions` - Chat format (what we use)
- `/v1/completions` - Raw text completion
- `/health` - Server health check
- `/v1/models` - List available models

**Our current code uses OpenAI chat format** - should work seamlessly with vLLM!

### Error Handling

**Graceful Degradation**:
- If visualizer fails → continue without descriptions
- If DM fails → show standard responses
- If AI player fails → return to manual control
- If translator fails → accept raw commands (fast-path)

**Health Monitoring**:
- Periodic health checks
- Automatic retry with backoff
- Fallback to mock/disabled mode
- User notification of degraded functionality

## Testing Strategy

### Unit Tests
- LLM router endpoint selection
- Concurrent request handling
- Error handling and fallbacks

### Integration Tests
- End-to-end gameplay with all LLMs active
- Performance benchmarks (latency, throughput)
- Stability tests (extended sessions)

### Acceptance Tests
- [ ] Input translation works with vLLM
- [ ] Scene descriptions generate accurately
- [ ] Dungeon Master enhances without breaking game
- [ ] AI Player can complete at least 10 moves
- [ ] System runs stably for 30+ minutes

## Documentation

### User Guides
- Setup instructions for vLLM servers
- Configuration options for each LLM
- Troubleshooting common issues

### Developer Guides
- Adding new LLM endpoints
- Creating new AI agents
- Prompt engineering best practices

## Future Vision

### Beyond Zork
This architecture could support:
- Other Z-machine games (Hitchhiker's Guide, Planetfall, etc.)
- Modern interactive fiction (Inform 7 games)
- Custom text adventures with AI enhancements

### Research Opportunities
- Multi-agent AI collaboration
- Emergent storytelling from AI interactions
- Performance benchmarks for Tenstorrent inference
- Educational platform for AI/gaming integration

## References

- Phase 2 LLM Integration: `/home/ttuser/code/tt-zork1/docs/CONTEXT_FREE_MODE.md`
- vLLM Documentation: https://docs.vllm.ai/
- OpenAI API Spec: https://platform.openai.com/docs/api-reference
- TT CLI Guide: `tt --help`

---

**Let's build the future of interactive fiction on AI hardware!** 🎮🤖✨
