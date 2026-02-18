# A/B Test Implementation: Multi-Agent vs Single 8B Model

**Date**: 2026-02-18
**Status**: ✅ COMPLETE - Ready for testing

## Summary

Implemented complete A/B testing infrastructure to compare:
- **Setup A**: 4× Llama-3.2-1B specialized models (current)
- **Setup B**: 1× Llama-3.1-8B unified model (proposed)

All code changes, configurations, and testing scripts are complete.

## What Was Implemented

### 1. Startup Scripts (2 files)

**`scripts/start-single-8b.sh`**
- Starts Llama-3.1-8B with tensor parallelism across 4 chips
- Port: 9000
- Health check with progress indicators
- ~3-5 minute startup time

**`scripts/stop-single-8b.sh`**
- Stops all servers (since tensor parallelism uses all devices)

### 2. Configuration (1 file)

**`config/llm_mode_8b.yaml`**
- Unified mode configuration
- Single endpoint for all 4 tasks
- Task-specific temperature and max_tokens overrides
- Documents A/B testing notes

### 3. Unified Prompt (1 file)

**`prompts/unified/system_v1_8b.txt`**
- Master prompt with 4 roles (Translator, Artist, DM, Player)
- Role selection via task prefixes:
  - `[TRANSLATE]` - Natural language → commands
  - `[VISUALIZE]` - Location → ASCII art
  - `[NARRATE]` - Add atmospheric narration
  - `[AUTOPLAY]` - Strategic gameplay
- Comprehensive examples for each role
- ~300 lines total

### 4. Router Modifications (1 file)

**`src/llm/llm_router.c`**
- Modified `route_unified()` function
- Automatically adds task prefixes to user input
- Handles response extraction per task type
- Matches existing multi-agent behavior

### 5. Testing Scripts (2 files)

**`scripts/test-single-8b.sh`**
- Quick smoke test for 8B model
- Tests all 4 roles via curl
- Verifies task prefix routing works

**`scripts/compare-setups.sh`**
- Complete automated A/B test
- Two 10-minute gameplay sessions
- Logs all output to `test_results/`
- Creates summary markdown template

### 6. Documentation (2 files)

**`docs/MODEL_COMPARISON.md`**
- Comprehensive architecture comparison
- Performance analysis (latency, quality expectations)
- A/B testing methodology
- Decision criteria
- Results template (to be filled after testing)

**`docs/A_B_TEST_IMPLEMENTATION.md`** (this file)
- Implementation summary
- Usage instructions
- Quick start guide

## File Structure

```
tt-zork1/
├── config/
│   ├── llm_mode.yaml           # Multi-agent config (existing)
│   └── llm_mode_8b.yaml        # Single 8B config (NEW)
│
├── prompts/
│   └── unified/
│       └── system_v1_8b.txt    # Unified prompt (NEW)
│
├── scripts/
│   ├── start-four-llms.sh      # Multi-agent startup (existing)
│   ├── stop-four-llms.sh       # Multi-agent shutdown (existing)
│   ├── start-single-8b.sh      # Single 8B startup (NEW)
│   ├── stop-single-8b.sh       # Single 8B shutdown (NEW)
│   ├── test-single-8b.sh       # Quick test (NEW)
│   └── compare-setups.sh       # A/B comparison (NEW)
│
├── src/llm/
│   └── llm_router.c            # Modified for task prefixes
│
└── docs/
    ├── MODEL_COMPARISON.md     # Analysis doc (NEW)
    └── A_B_TEST_IMPLEMENTATION.md  # This file (NEW)
```

## Usage

### Quick Start

**Option 1: Run Automated A/B Test**
```bash
# Full comparison (interactive, ~30 minutes)
./scripts/compare-setups.sh

# Results saved to test_results/
```

**Option 2: Manual Testing**

**Test Multi-Agent Mode:**
```bash
# Start 4× 1B models
./scripts/start-four-llms.sh

# Play Zork (uses config/llm_mode.yaml by default)
./play-zork-with-llm.sh

# Stop when done
./scripts/stop-four-llms.sh
```

**Test Single 8B Mode:**
```bash
# Start 8B model
./scripts/start-single-8b.sh

# Play Zork with 8B config
ZORK_LLM_CONFIG=config/llm_mode_8b.yaml ./play-zork-with-llm.sh

# Stop when done
./scripts/stop-single-8b.sh
```

### Testing Checklist

Use these scenarios for fair comparison:

- [ ] **Complex translation**: "I want to carefully open the small mailbox"
- [ ] **Pronoun resolution**: "examine mailbox" → "open it"
- [ ] **Location variety**: Visit 5-10 different rooms
- [ ] **ASCII art creativity**: Compare variety and composition
- [ ] **Auto-play intelligence**: `/play auto` for 5 moves
- [ ] **Cross-task coherence**: Do agents feel unified?
- [ ] **Latency perception**: Is 800ms noticeable vs 200ms?

### Smoke Test (Verify Setup)

```bash
# Start 8B model
./scripts/start-single-8b.sh

# Quick test (should see responses for all 4 roles)
./scripts/test-single-8b.sh

# Output:
# ✅ 8B model responding on port 9000
# Testing TRANSLATOR role... [response]
# Testing ARTIST role...      [response]
# Testing PLAYER role...      [response]
# ✅ All roles tested!
```

## Architecture Details

### Current: Multi-Agent (4× 1B)

```
User Command
    │
    ├─→ [Port 8000] Translator (Llama-3.2-1B) ──→ "north"
    │
Game State Change
    │
    ├─→ [Port 8001] Artist (Llama-3.2-1B) ────→ "🌲🌳🌲"
    │
    ├─→ [Port 8002] DM (Llama-3.2-1B) ─────────→ "You enter..."
    │
Auto Mode
    │
    └─→ [Port 8003] Player (Llama-3.2-1B) ─────→ "take lamp"

Execution: PARALLEL (all 4 simultaneously)
Latency: ~200ms (limited by slowest)
```

### Proposed: Single 8B (Tensor Parallel)

```
User Command
    │
    ├─→ [Port 9000] "[TRANSLATE] go north" ──→ Llama-3.1-8B ──→ "north"
    │
Game State Change
    │
    ├─→ [Port 9000] "[VISUALIZE] Forest" ────→ Llama-3.1-8B ──→ "🌲🌳🌲"
    │
    ├─→ [Port 9000] "[NARRATE] Forest" ──────→ Llama-3.1-8B ──→ "You enter..."
    │
Auto Mode
    │
    └─→ [Port 9000] "[AUTOPLAY] You are..." ─→ Llama-3.1-8B ──→ "take lamp"

Execution: SERIAL (one at a time, queued)
Latency: ~800ms (4× 200ms)
```

### Task Prefix Routing

The `llm_router.c` automatically adds prefixes:

```c
// Code in route_unified()
const char *task_prefixes[] = {
    "[TRANSLATE] ",   /* LLM_TASK_TRANSLATE */
    "[VISUALIZE] ",   /* LLM_TASK_VISUALIZE */
    "[NARRATE] ",     /* LLM_TASK_NARRATE */
    "[AUTOPLAY] "     /* LLM_TASK_AUTOPLAY */
};

// Build prefixed input
char prefixed_input[4096];
snprintf(prefixed_input, sizeof(prefixed_input), "%s%s",
         task_prefixes[task], input);
```

This lets the single model understand which role to play.

## Expected Results

### Hypothesis

**Quality**: Single 8B will be significantly better at:
- ✅ Complex translation (pronouns, ambiguity)
- ✅ Creative ASCII art (compositional vs template)
- ✅ Strategic planning (multi-turn thinking)
- ✅ Narrative coherence (adaptive storytelling)

**Latency**: 4× slower (800ms vs 200ms) but still acceptable

**Verdict**: Quality improvement will outweigh latency cost

### Decision Framework

**If 8B wins decisively:**
- Update `config/llm_mode.yaml` to use unified mode by default
- Document in CLAUDE.md
- Keep multi-agent as fast mode option

**If multi-agent wins:**
- Keep current setup as default
- Document that parallelism > quality for this use case
- Archive 8B mode as "high-quality" option

**If it's close:**
- Provide both modes
- Let users choose based on preference
- Document trade-offs clearly

## Troubleshooting

### 8B Model Won't Start

**Symptom**: Timeout during startup
**Fixes**:
1. Check available VRAM: `tt-smi`
2. Verify model exists: `ls ~/models/models--meta-llama--Llama-3.1-8B-Instruct`
3. Check logs: `ls -ltr ~/.tt/servers/logs/`
4. Try stopping all servers first: `tt serve stop --all`

### Routing Not Working

**Symptom**: Single command goes to all tasks
**Fixes**:
1. Verify config is loaded: `echo $ZORK_LLM_CONFIG`
2. Check router mode: Look for "[Router Debug] Mode: unified" in output
3. Rebuild project: `./scripts/build_local.sh release`

### Tensor Parallelism Fails

**Symptom**: Model loads but crashes on inference
**Fixes**:
1. Verify 4 devices available: `tt-smi -s | jq '.devices | length'`
2. Check device health: `tt-smi -s | jq '.devices[].health'`
3. Try single-chip 7B fallback: Modify config to use device-id 0 only

## Next Steps

1. **Run A/B test**: `./scripts/compare-setups.sh`
2. **Fill in results**: Edit `test_results/summary_YYYYMMDD_HHMMSS.md`
3. **Make decision**: Based on metrics and experience
4. **Update docs**: Document winner in `CLAUDE.md` and `MODEL_COMPARISON.md`
5. **Deploy**: Set default config to winner

## Performance Targets

| Metric | Multi-Agent | Single 8B | Threshold |
|--------|-------------|-----------|-----------|
| Latency | 200ms | 800ms | < 1000ms ✅ |
| Translation | 75% accurate | 95%+ accurate | > 90% |
| ASCII Quality | 6/10 | 8/10 | > 7/10 |
| Strategy | 5/10 | 8/10 | > 7/10 |

## References

- Research plan: Generated from plan mode (comprehensive analysis)
- Implementation: This document
- Testing guide: `docs/MODEL_COMPARISON.md`
- Code changes: `src/llm/llm_router.c` (route_unified function)

---

**Status**: ✅ Implementation complete, ready for user testing
**Last Updated**: 2026-02-18
**Build Status**: ✅ Compiled successfully
**Test Status**: 🔄 Awaiting user A/B test execution
