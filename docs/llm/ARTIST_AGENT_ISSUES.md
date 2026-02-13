# Artist Agent Issues with Qwen3-0.6B

## Problem Summary

The Qwen3-0.6B model is **not suitable** for direct ASCII art generation. Testing shows that regardless of prompt engineering, the model consistently spends all available tokens on "thinking" about the task rather than generating the actual ASCII art output.

## Test Results

Multiple prompt strategies were tested:

| Approach | Result |
|----------|--------|
| Direct instructions ("Output ONLY art") | ❌ Model explains task |
| Examples-only (no instructions) | ❌ Model analyzes examples |
| Ultra-minimal prompt | ❌ Model thinks about approach |
| Completion-style pattern | ❌ Model interprets instead of completing |
| Temperature = 0.0 | ❌ Still generates thinking text |
| Max tokens = 80-500 | ❌ All tokens used for thinking |

**Raw Output Example (with 500 max_tokens):**
```
<think>,Okay, the user is asking about the West of House. Let me think about
how to respond. The forest and cave environments are provided, so maybe I can
blend elements from both. The forest part uses 🌲 and the cave uses ⓑ, which
are symbols. Maybe I should use them in a way that combines both...
```

**No ASCII art is ever generated** - 100% of tokens used for meta-reasoning.

## Root Cause

The Qwen3-0.6B model is instruction-tuned for conversational AI with chain-of-thought reasoning. This makes it excellent for tasks requiring explanation but poor for direct creative generation without explanation.

The model has been trained to:
1. Understand the user's request
2. Plan an approach
3. Explain its reasoning
4. Generate output

For ASCII art, we only want step 4, but the model always executes steps 1-3 first and runs out of tokens.

## Solutions

### Option 1: Disable Artist Agent (Recommended for Now)

Set in `config/llm_mode.yaml`:
```yaml
# Set ZORK_ARTIST_ENABLED=0 to disable
# Or comment out artist endpoint
```

Or set environment variable:
```bash
export ZORK_ARTIST_ENABLED=0
```

### Option 2: Use a Larger Model

Test with qwen2.5:1.5b or qwen2.5:3b which may be better at following "no thinking" instructions:

```bash
# Terminal 1
vllm serve /home/ttuser/models/Qwen2.5-1.5B --port 8001

# Update config/llm_mode.yaml
artist:
  model: /home/ttuser/models/Qwen2.5-1.5B
  max_tokens: 150
```

### Option 3: Use a Different Model Architecture

Try models specifically trained for completion rather than chat:
- Code generation models (often better at pattern completion)
- Base models (non-instruct versions)
- Models trained with less chain-of-thought emphasis

### Option 4: Post-Process to Extract Art

Implement a filter function that:
1. Searches response for lines containing emojis or box-drawing characters
2. Extracts only those lines
3. Discards thinking text

**Caveat:** Current testing shows zero art generation, so there's nothing to extract.

### Option 5: Placeholder Art

Generate simple fallback art based on keywords:

```
West of House → 🏠
Forest → 🌲🌳
Cave → 🏔️💀
```

## Current Status

- ✅ **Translator Agent (Chip 0)**: WORKING - Single-line command extraction works well
- ⚠️ **Artist Agent (Chip 1)**: BLOCKED - Model unsuitable for task
- ⏳ **DM Agent (Chip 2)**: Not yet tested
- ⏳ **Player Agent (Chip 3)**: Not yet tested

## Recommendation

**Proceed with DM and Player agents** which generate natural language text rather than structured ASCII art. These may work better with Qwen3-0.6B's conversational training. Come back to artist with a different model or approach later.

## Files Tested

Prompt variations tried:
- `prompts/artist/system_v3_direct.txt` - Direct instructions
- `prompts/artist/system_v4_ultra_direct.txt` - Ultra-minimal
- `prompts/artist/system_v5_examples_only.txt` - Examples without instructions
- `prompts/artist/system_v6_minimal_stop.txt` - Minimal with stop sequences
- `prompts/artist/system_v7_completion.txt` - Completion-style pattern

All exhibited the same "thinking instead of generating" behavior.

## Updated: 2026-02-13
