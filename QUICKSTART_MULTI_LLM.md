# Quick Start: 4-LLM Zork Architecture

**Status**: Ready for Testing
**Branch**: `feature/multi-llm-integration`
**Hardware Required**: 4× Tenstorrent P300C chips

## What We Built

A multi-agent AI system for Zork using **4 specialized LLMs** (one per chip):

| Chip | Port | Purpose | Status |
|------|------|---------|--------|
| 0 | 8000 | **Command Translator** | ✅ Ready |
| 1 | 8001 | **ASCII Artist** | ✅ Ready |
| 2 | 8002 | **Dungeon Master** | ✅ Ready |
| 3 | 8003 | **AI Player** | ✅ Ready |

Each LLM runs **Qwen3-0.6B** independently for maximum throughput.

---

## 5-Minute Quick Start

### 1. Start All LLM Servers (~5 min first run)

```bash
cd /home/ttuser/code/tt-zork1

# Start 4 independent vLLM servers
./scripts/start-four-llms.sh
```

**First run**: Compilation takes 2-5 minutes per chip (cached after)
**Subsequent runs**: ~30 seconds startup

---

### 2. Test All Endpoints (~1 min)

```bash
# Verify all 4 LLMs are responding
./scripts/test-four-llms.sh
```

**Expected output**:
```
[Translator] Testing on port 8000...
  ✓ Health check passed
  ✓ Inference successful
  Response: open mailbox

[Artist] Testing on port 8001...
  ✓ Health check passed
  ✓ Inference successful

[DM] Testing on port 8002...
  ✓ Health check passed
  ✓ Inference successful

[Player] Testing on port 8003...
  ✓ Health check passed
  ✓ Inference successful

✅ Testing complete!
```

---

### 3. Experiment with Prompts (~10 min)

```bash
# Test different prompt variants
./scripts/test-prompt-variants.sh

# Results saved to experiments/prompt_variants_TIMESTAMP/
# Review translator_*.txt, artist_*.txt files
```

Or test interactively:

```bash
# Real-time prompt tuning
./scripts/tune-prompt-interactive.sh

# Select LLM (1-4)
1

# Test inputs
> test open the mailbox
> test I want to pick up the lamp
> test go north and take sword

# Adjust parameters
> temp 0.2
> tokens 100

# Test again
> test open the mailbox

# Save if better
> save translator_optimized
```

---

### 4. Compare Variants Side-by-Side

```bash
# Compare translator variants
./scripts/compare-prompts.sh translator "open the mailbox"

# Compare artist variants
./scripts/compare-prompts.sh artist "West of House"
```

---

### 5. Stop Servers

```bash
./scripts/stop-four-llms.sh
```

---

## What's Next

### Phase 1: Prompt Optimization ⏳ In Progress

1. ✅ Create prompt variants (done)
2. ⏳ **Run tests on hardware** ← YOU ARE HERE
3. ⏳ Analyze results
4. ⏳ Pick optimal variants
5. ⏳ Update config.yaml

**Action**: Run `./scripts/start-four-llms.sh` then `./scripts/test-prompt-variants.sh`

---

### Phase 2: LLM Router Module

Create `src/llm/llm_router.{h,c}` to dispatch requests to specialized endpoints.

**Key functions**:
```c
llm_router_init()
llm_router_request(LLM_TASK_TRANSLATE, "open mailbox", buffer, size)
llm_router_request(LLM_TASK_VISUALIZE, scene_json, buffer, size)
```

---

### Phase 3: ASCII Artist Integration

Create `src/llm/scene_visualizer.{h,c}` to generate ASCII art on location changes.

**Display modes**:
- Above description (default)
- Sidebar (wide terminals)
- On-demand (`visualize` command)

---

### Phase 4: Dungeon Master Integration

Create `src/llm/dungeon_master.{h,c}` to add atmospheric flavor text.

**Enhancements**:
- Reference recent actions
- Add sensory details
- Match location mood
- Never change game state

---

### Phase 5: AI Player Integration

Create `src/llm/ai_player.{h,c}` for autonomous gameplay.

**Modes**:
- Observer: Suggest moves, human decides
- Co-pilot: AI plays, human can interrupt
- Autonomous: Fully automatic
- Teaching: Explain reasoning

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────┐
│                   Zork Game Engine                   │
│             (Z-machine Interpreter)                  │
└──────────────────┬──────────────────────────────────┘
                   │
          ┌────────┴────────┐
          │   LLM Router    │
          │  (Dispatcher)   │
          └─────────────────┘
                   │
      ┌────────────┼────────────┬────────────┐
      │            │            │            │
   Port 8000    Port 8001    Port 8002    Port 8003
      │            │            │            │
┌─────┴─────┐┌────┴────┐┌─────┴─────┐┌────┴─────┐
│Translator ││  Artist  ││    DM     ││  Player  │
│ Qwen 0.6B ││Qwen 0.6B││ Qwen 0.6B ││Qwen 0.6B │
│ Device 0  ││Device 1 ││ Device 2  ││Device 3  │
└───────────┘└─────────┘└───────────┘└──────────┘
```

---

## File Organization

```
tt-zork1/
├── docs/
│   ├── FOUR_CHIP_ARCHITECTURE.md      # Complete architecture
│   ├── MULTI_LLM_ARCHITECTURE.md      # Original vision
│   └── PROMPT_ENGINEERING_GUIDE.md    # Comprehensive guide
├── prompts/
│   ├── config.yaml                     # Configuration
│   ├── translator/                     # 3 variants
│   ├── artist/                         # 2 variants
│   ├── dm/                             # 2 variants
│   └── player/                         # 2 variants
├── scripts/
│   ├── start-four-llms.sh             # Start all servers
│   ├── stop-four-llms.sh              # Stop all servers
│   ├── test-four-llms.sh              # Health check
│   ├── test-prompt-variants.sh        # Automated testing
│   ├── compare-prompts.sh             # Side-by-side comparison
│   └── tune-prompt-interactive.sh     # Interactive tuning
└── src/llm/                            # (Future C modules)
    ├── llm_router.{h,c}               # Router module
    ├── scene_visualizer.{h,c}         # ASCII artist
    ├── dungeon_master.{h,c}           # DM enhancement
    └── ai_player.{h,c}                # AI player
```

---

## Configuration Presets

Edit `prompts/config.yaml` to change behavior:

### Speed Optimized
```yaml
translator: minimal
artist: simple
dm: off
player: off
```
**Use when**: You want fastest possible responses

---

### Balanced (Recommended) ⭐
```yaml
translator: fewshot
artist: atmospheric
dm: subtle
player: off
```
**Use when**: Good balance of quality and speed

---

### Immersive
```yaml
translator: fewshot
artist: atmospheric
dm: dramatic
player: strategic
```
**Use when**: Maximum AI enhancement

---

### AI Demo
```yaml
translator: fewshot
artist: atmospheric
dm: dramatic
player: strategic (teaching mode)
```
**Use when**: Showcasing AI capabilities

---

## Performance Targets

| LLM | Target Latency | Max Tokens | Temperature |
|-----|---------------|------------|-------------|
| Translator | <200ms | 75 | 0.15 |
| Artist | <1000ms | 400 | 0.8 |
| DM | <1500ms | 200 | 0.9 |
| Player | <2000ms | 400 | 0.7 |

---

## Troubleshooting

### Servers won't start

```bash
# Check hardware
tt device info

# Check for stuck processes
ps aux | grep vllm

# Kill stuck servers
pkill -f vllm

# Try again
./scripts/start-four-llms.sh
```

---

### Server times out during compilation

**Normal**: First run compiles kernels (2-5 min per chip)

**If stuck >10 min**:
```bash
# Check logs
tail -f ~/.tt/servers/logs/qwen3-0.6b.log

# If truly stuck, kill and restart
./scripts/stop-four-llms.sh
./scripts/start-four-llms.sh
```

---

### High latency

1. Check server status: `tt serve status`
2. Reduce max_tokens in config.yaml
3. Use simpler prompt variants
4. Enable caching

---

### Inaccurate translations

1. Test different variants: `./scripts/compare-prompts.sh translator "input"`
2. Try lower temperature (0.1 instead of 0.15)
3. Add more examples to prompt
4. Test with real user inputs

---

## Resources

### Documentation
- **Architecture**: `docs/FOUR_CHIP_ARCHITECTURE.md`
- **Prompt Guide**: `docs/PROMPT_ENGINEERING_GUIDE.md`
- **Prompts README**: `prompts/README.md`

### Scripts
- **Start**: `./scripts/start-four-llms.sh`
- **Test**: `./scripts/test-four-llms.sh`
- **Tune**: `./scripts/tune-prompt-interactive.sh`

### Hardware
- **Device Info**: `tt device info`
- **Server Status**: `tt serve status`

---

## Success Checklist

- [ ] All 4 servers start successfully
- [ ] Health checks pass for all endpoints
- [ ] Test translations are accurate (>90%)
- [ ] ASCII art is recognizable
- [ ] Latencies meet targets
- [ ] No crashes during gameplay
- [ ] Graceful degradation on failures

---

## What You Can Do Right Now

1. **Start servers**: `./scripts/start-four-llms.sh`
2. **Test health**: `./scripts/test-four-llms.sh`
3. **Experiment**: `./scripts/tune-prompt-interactive.sh`
4. **Compare**: `./scripts/compare-prompts.sh translator "test input"`
5. **Optimize**: Pick best variants and update config.yaml

---

**Questions?** See:
- `docs/PROMPT_ENGINEERING_GUIDE.md` for detailed guidance
- `docs/FOUR_CHIP_ARCHITECTURE.md` for complete architecture
- `prompts/README.md` for prompt reference

**Let's build the future of interactive fiction!** 🎮🤖✨
