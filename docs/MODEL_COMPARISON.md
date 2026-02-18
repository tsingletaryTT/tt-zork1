# Model Architecture Comparison: Multi-Agent vs Single Large Model

## Overview

This document compares two LLM deployment architectures for the Zork AI system:

- **Architecture A**: Multi-Agent (4× small specialized models)
- **Architecture B**: Single Large Model (1× large unified model)

## Architectures

### Architecture A: Multi-Agent (Current)

**Configuration:**
- **Models**: 4× Llama-3.2-1B-Instruct
- **Deployment**: One model per P300C chip (devices 0-3)
- **Ports**: 8000 (Translator), 8001 (Artist), 8002 (DM), 8003 (Player)
- **Execution**: Parallel (all 4 agents run simultaneously)
- **Total Parameters**: ~4B (1B × 4 models)

**Specialization:**
| Agent | Model | Prompt Size | Task |
|-------|-------|-------------|------|
| Translator | Llama-3.2-1B | 61 lines | Natural language → commands |
| Artist | Llama-3.2-1B | 53 lines | Location → ASCII art |
| DM | Llama-3.2-1B | ~80 lines | Add narrative atmosphere |
| Player | Llama-3.2-1B | 53-193 lines | Autonomous strategic play |

**Strengths:**
- ✅ Parallel execution (~200ms per turn, all agents simultaneously)
- ✅ Specialized prompts optimized per task
- ✅ Low memory per model (~1-2GB each)
- ✅ Proven stable on hardware
- ✅ Independent failure (one agent fails, others continue)

**Weaknesses:**
- ❌ Limited reasoning capability (1B parameters)
- ❌ Context-free mode required (can't use game history)
- ❌ No cross-task coherence
- ❌ Struggles with complex disambiguation
- ❌ Template-based ASCII art (less creative)

### Architecture B: Single Large Model (Proposed)

**Configuration:**
- **Model**: 1× Llama-3.1-8B-Instruct
- **Deployment**: Tensor parallel across 4 chips (devices 0-3)
- **Port**: 9000 (single endpoint for all tasks)
- **Execution**: Serial (4 agents queued, execute sequentially)
- **Total Parameters**: ~8B (single unified model)

**Task Routing:**
Uses role prefixes in unified prompt:
- `[TRANSLATE]` - Translation role
- `[VISUALIZE]` - ASCII art role
- `[NARRATE]` - Dungeon master role
- `[AUTOPLAY]` - Player agent role

**Strengths:**
- ✅ Superior reasoning (8B parameters, 2× smarter than 4× 1B)
- ✅ Can use rich context (game history, cross-task understanding)
- ✅ Better creative generation (compositional ASCII art)
- ✅ Improved disambiguation and pronoun resolution
- ✅ Strategic depth (player agent can plan ahead)
- ✅ Unified experience (all agents "know" each other)

**Weaknesses:**
- ❌ Serial execution (~800ms per turn vs 200ms parallel)
- ❌ Single point of failure (model crash = all agents down)
- ❌ More complex deployment (tensor parallelism)
- ❌ Larger context window pressure (all 4 prompts in one)

## Performance Analysis

### Latency Comparison

**Multi-Agent (Parallel):**
```
Turn Start
  ├─ Translator (50ms)  ─────┐
  ├─ Artist (100ms)     ─────┤  All parallel
  ├─ DM (150ms)         ─────┤  = 200ms total
  └─ Player (200ms)     ─────┘
Turn End: ~200ms
```

**Single 8B (Serial):**
```
Turn Start
  ├─ Translator (200ms) ──→ Artist (200ms) ──→ DM (200ms) ──→ Player (200ms)
Turn End: ~800ms
```

**Verdict**: 4× slower, but still < 1 second (acceptable for showcase)

### Quality Expectations

| Metric | Multi-Agent (4× 1B) | Single 8B |
|--------|---------------------|-----------|
| Translation | Good (context-free) | Excellent (context-aware) |
| ASCII Art | Template-based | Creative/compositional |
| DM Narrative | Basic atmosphere | Rich, adaptive storytelling |
| Player Strategy | Simple heuristics | Multi-turn planning |
| Pronoun Resolution | Fails without context | Understands references |
| Ambiguity Handling | Literal only | Infers intent |

## A/B Testing Methodology

### Test Protocol

**Duration**: Two 10-minute gameplay sessions

**Session 1**: Multi-Agent mode
```bash
./scripts/start-four-llms.sh
./play-zork-with-llm.sh
```

**Session 2**: Single 8B mode
```bash
./scripts/start-single-8b.sh
ZORK_LLM_CONFIG=config/llm_mode_8b.yaml ./play-zork-with-llm.sh
```

### Test Scenarios

1. **Complex Translation**
   - Input: "I want to carefully open the small mailbox"
   - Measure: Does it preserve "carefully"? Handle "small"?

2. **Pronoun Resolution**
   - Sequence: "examine mailbox" → "open it"
   - Measure: Does "it" resolve to "mailbox"?

3. **ASCII Art Creativity**
   - Visit 5-10 locations
   - Measure: Template reuse vs unique composition

4. **Strategic Planning**
   - Enable `/play auto` for 5 moves
   - Measure: Does agent exhibit coherent strategy?

5. **Cross-Task Coherence**
   - Player encounters troll → DM narrates → Artist visualizes
   - Measure: Unified experience vs disjointed?

### Metrics to Collect

**Quantitative:**
- Latency: Average time from command to response
- Translation accuracy: Successful translations / total attempts
- Memory usage: VRAM per chip
- Model utilization: GPU utilization %

**Qualitative:**
- Translation: Handles edge cases? (scale 1-10)
- ASCII art: Creativity and variety (scale 1-10)
- Player intelligence: Strategic depth (scale 1-10)
- Overall experience: Which feels more magical? (subjective)

## Running the Comparison

### Automated A/B Test

```bash
# Full comparison script (interactive)
./scripts/compare-setups.sh

# Results saved to:
test_results/test1_multi_agent_YYYYMMDD_HHMMSS.log
test_results/test2_single_8b_YYYYMMDD_HHMMSS.log
test_results/summary_YYYYMMDD_HHMMSS.md
```

### Manual Testing

**Option 1: Multi-Agent**
```bash
./scripts/start-four-llms.sh
./play-zork-with-llm.sh
# Play for 10 minutes
./scripts/stop-four-llms.sh
```

**Option 2: Single 8B**
```bash
./scripts/start-single-8b.sh
ZORK_LLM_CONFIG=config/llm_mode_8b.yaml ./play-zork-with-llm.sh
# Play for 10 minutes
./scripts/stop-single-8b.sh
```

## Decision Criteria

**Choose Single 8B if:**
- Quality improvement > 30%
- Latency still acceptable (< 1s per turn)
- Creative tasks significantly better
- Cross-task coherence valuable

**Choose Multi-Agent if:**
- Latency matters more than quality
- Parallelism critical for responsiveness
- Simplicity valued over sophistication
- Current quality sufficient

**Hybrid Option:**
- Lightweight tasks (Translator): 1B on single chip
- Heavy tasks (Artist, DM, Player): 7B across 3 chips
- Best of both worlds?

## Results

### Test Date: [TBD]

#### Session 1: Multi-Agent (4× 1B)

**Translation Quality**: [TBD]
- Accuracy: X/Y commands successful
- Edge cases: [observations]

**ASCII Art**: [TBD]
- Creativity rating: X/10
- Variety: [observations]

**Player Strategy**: [TBD]
- Intelligence rating: X/10
- Progress: [observations]

**Latency**: [TBD]
- Average: Xms per turn
- Subjective feel: [instant/fast/acceptable/slow]

#### Session 2: Single 8B

**Translation Quality**: [TBD]
- Accuracy: X/Y commands successful
- Edge cases: [observations]

**ASCII Art**: [TBD]
- Creativity rating: X/10
- Variety: [observations]

**Player Strategy**: [TBD]
- Intelligence rating: X/10
- Progress: [observations]

**Latency**: [TBD]
- Average: Xms per turn
- Subjective feel: [instant/fast/acceptable/slow]

### Recommendation: [TBD]

**Winner**: [Multi-Agent / Single 8B / Hybrid]

**Reasoning**: [TBD]

**Action Items**:
- [ ] Update default config to winner
- [ ] Document decision in CLAUDE.md
- [ ] Create user-selectable mode if close call
- [ ] Archive or maintain losing setup as option

## References

- Configuration files:
  - `config/llm_mode.yaml` - Multi-agent setup
  - `config/llm_mode_8b.yaml` - Single 8B setup
- Prompts:
  - `prompts/translator/system_v5_magic.txt` - Translator (multi-agent)
  - `prompts/artist/system_v9_magic.txt` - Artist (multi-agent)
  - `prompts/unified/system_v1_8b.txt` - Unified prompt (single model)
- Scripts:
  - `scripts/start-four-llms.sh` - Start multi-agent
  - `scripts/start-single-8b.sh` - Start single 8B
  - `scripts/compare-setups.sh` - Automated A/B test
- Implementation:
  - `src/llm/llm_router.c` - Request routing logic

## Notes

- Single 8B model is **hypothesis-driven**: We expect quality to outweigh latency cost
- Both setups use same hardware (4× P300C chips)
- Tensor parallelism is proven technology (tested with llama-3.1-8b elsewhere)
- Can provide both modes as user option if test is inconclusive

---

**Status**: Ready for A/B testing
**Last Updated**: 2026-02-18
