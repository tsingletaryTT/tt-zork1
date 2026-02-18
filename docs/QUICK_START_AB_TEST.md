# Quick Start: A/B Testing Multi-Agent vs Single 8B

## TL;DR

```bash
# Run complete A/B test (30 minutes interactive)
./scripts/compare-setups.sh

# OR test each mode manually:

# Test 1: Multi-Agent (4× 1B, parallel, fast)
./scripts/start-four-llms.sh
./play-zork-with-llm.sh
./scripts/stop-four-llms.sh

# Test 2: Single 8B (1× 8B, serial, smart)
./scripts/start-single-8b.sh
ZORK_LLM_CONFIG=config/llm_mode_8b.yaml ./play-zork-with-llm.sh
./scripts/stop-single-8b.sh
```

## What Are We Testing?

### Setup A: Multi-Agent (Current)
- **Models**: 4× Llama-3.2-1B (one per chip)
- **Speed**: ~200ms per turn (parallel)
- **Quality**: Good for simple tasks
- **Limitation**: Context-free, no cross-task awareness

### Setup B: Single 8B (Proposed)
- **Model**: 1× Llama-3.1-8B (across all 4 chips)
- **Speed**: ~800ms per turn (serial)
- **Quality**: Superior reasoning and creativity
- **Advantage**: Context-aware, unified experience

## Visual Comparison

```
┌─────────────────────────────────────────────────────────────┐
│  SETUP A: Multi-Agent (4× 1B)                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Chip 0: Translator (1B)  ──┐                              │
│  Chip 1: Artist (1B)      ──┤──→ Parallel execution        │
│  Chip 2: DM (1B)          ──┤    (all run simultaneously)  │
│  Chip 3: Player (1B)      ──┘                              │
│                                                             │
│  Latency: 200ms  |  Quality: Good  |  Mode: Independent   │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  SETUP B: Single 8B (Tensor Parallel)                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  All 4 chips: One 8B Model                                 │
│    │                                                        │
│    ├─→ [TRANSLATE] role ──→ 200ms                          │
│    ├─→ [VISUALIZE] role ──→ 200ms                          │
│    ├─→ [NARRATE] role   ──→ 200ms                          │
│    └─→ [AUTOPLAY] role  ──→ 200ms                          │
│                             ─────                           │
│                             800ms total                     │
│                                                             │
│  Latency: 800ms  |  Quality: Excellent  |  Mode: Unified  │
└─────────────────────────────────────────────────────────────┘
```

## Test Scenarios (Use These for Fair Comparison)

### 1. Translation Quality
```
Try: "I want to carefully open the small mailbox"
     "take it" (after examining something)
     "go inside" (contextual direction)

Multi-Agent: Literal translation, no context
Single 8B:   Understands pronouns and context
```

### 2. ASCII Art Creativity
```
Visit: Forest, Dark Cave, Treasure Room, West of House

Multi-Agent: Template-based (similar patterns)
Single 8B:   Compositional (unique per location)
```

### 3. Player Intelligence
```
Try: /play auto (let AI play for 5 moves)

Multi-Agent: Simple heuristics
Single 8B:   Strategic planning
```

### 4. Response Feel
```
Measure: Does 800ms feel slow compared to 200ms?

Target: Both should feel < 1 second (instant)
```

## What to Look For

### Multi-Agent Strengths
- ✅ Instant response (< 200ms)
- ✅ Simple and reliable
- ✅ Good for straightforward commands

### Multi-Agent Weaknesses
- ❌ "take it" → "I don't understand 'it'"
- ❌ ASCII art reuses templates
- ❌ AI player makes obvious mistakes

### Single 8B Strengths
- ✅ Understands "it", "that", "there"
- ✅ Creative unique ASCII art
- ✅ Smart strategic gameplay
- ✅ Agents feel coordinated

### Single 8B Weaknesses
- ❌ 4× slower (but still < 1s)
- ❌ More complex deployment

## Decision Matrix

| Criterion | Weight | Multi-Agent | Single 8B |
|-----------|--------|-------------|-----------|
| Translation Quality | High | 7/10 | 9/10 |
| ASCII Creativity | Medium | 6/10 | 8/10 |
| Player Strategy | Medium | 5/10 | 8/10 |
| Response Speed | High | 10/10 | 7/10 |
| User Experience | High | 7/10 | ? (test) |

**Hypothesis**: Single 8B wins if latency doesn't hurt UX

## Running the Test

### Automated (Recommended)
```bash
./scripts/compare-setups.sh

# This will:
# 1. Start multi-agent mode
# 2. Let you play for 10 minutes
# 3. Stop and start single 8B mode
# 4. Let you play for 10 minutes
# 5. Generate comparison report

# Results: test_results/summary_YYYYMMDD_HHMMSS.md
```

### Manual (If Automated Fails)
```bash
# Test 1: Multi-Agent
./scripts/start-four-llms.sh
./play-zork-with-llm.sh
# Play for 10 minutes, take notes
./scripts/stop-four-llms.sh

# Test 2: Single 8B
./scripts/start-single-8b.sh
ZORK_LLM_CONFIG=config/llm_mode_8b.yaml ./play-zork-with-llm.sh
# Play for 10 minutes, compare experience
./scripts/stop-single-8b.sh
```

## Expected Outcome

**Most Likely**: Single 8B wins
- Quality improvement > 30%
- Latency still acceptable (< 1s)
- Showcase benefits from "smarter" feel

**If Multi-Agent Wins**: Speed matters more than expected
- Keep current setup
- Offer 8B as "high-quality mode"

**If Close**: Provide both modes
- Default: Multi-agent (fast)
- Option: Single 8B (smart)
- User choice based on preference

## Files Created

| File | Purpose |
|------|---------|
| `scripts/start-single-8b.sh` | Start 8B with tensor parallelism |
| `scripts/stop-single-8b.sh` | Stop 8B server |
| `scripts/test-single-8b.sh` | Quick smoke test |
| `scripts/compare-setups.sh` | Full A/B test automation |
| `config/llm_mode_8b.yaml` | Single 8B configuration |
| `prompts/unified/system_v1_8b.txt` | Unified multi-role prompt |
| `docs/MODEL_COMPARISON.md` | Detailed analysis |
| `docs/A_B_TEST_IMPLEMENTATION.md` | Implementation guide |
| `src/llm/llm_router.c` | Modified (task prefixes) |

## Troubleshooting

**"8B model won't start"**
→ Check VRAM: `tt-smi`
→ Stop all servers: `tt serve stop --all`
→ Try again: `./scripts/start-single-8b.sh`

**"Routing not working"**
→ Verify config: `echo $ZORK_LLM_CONFIG`
→ Rebuild: `./scripts/build_local.sh release`

**"Too slow!"**
→ That's feedback! Document in results
→ May indicate multi-agent is winner

## Next Steps After Testing

1. **Fill in results**: `test_results/summary_*.md`
2. **Update default**: Set winner in `config/llm_mode.yaml`
3. **Document decision**: Update `CLAUDE.md`
4. **Consider hybrid**: Translator (1B) + others (7B)?

---

**Ready to test?** Run `./scripts/compare-setups.sh` and experience both modes!
