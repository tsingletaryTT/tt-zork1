# LLM-Powered Zork - Usage Guide

## Overview

Phase 2 adds LLM-powered natural language translation to the Zork interpreter. You can now play Zork using natural language instead of memorizing text adventure commands!

## Quick Start

### Mode 1: Mock Mode (Testing - No LLM Server Required)

Perfect for testing the integration without setting up an LLM server:

```bash
export ZORK_LLM_MOCK=1
./zork-native game/zork1.z3
```

**What happens:**
- First 4 inputs get translated using canned responses
- Great for verifying the integration works
- After 4 calls, falls back to pass-through mode

**Example session:**
```
> I want to open the mailbox
[LLM → open mailbox]
Opening the small mailbox reveals a leaflet.

> Please pick up the leaflet and read it
[LLM → take leaflet, read leaflet]
Taken.
"WELCOME TO ZORK! ..."

> Let's go north
[LLM → go north]
North of House
You are facing the north side of a white house...

> open the window and go inside
[LLM → open window, enter house]
The windows are boarded and can't be opened.
```

### Mode 2: Real LLM (Full Experience)

Use Ollama with Qwen2.5:0.5b for full natural language translation:

```bash
# Install and start Ollama (if not already running)
brew install ollama  # macOS
ollama pull qwen2.5:0.5b

# Run Zork with LLM (easy way)
./run-zork-llm.sh

# Or manually:
export ZORK_LLM_URL="http://localhost:11434/v1/chat/completions"
export ZORK_LLM_MODEL="qwen2.5:0.5b"
./zork-native game/zork1.z3
```

**What happens:**
- Every natural language input is sent to the LLM
- LLM translates based on full conversation context
- You can play indefinitely using natural language

### Mode 3: Disabled (Classic Mode)

Play classic Zork without LLM assistance:

```bash
export ZORK_LLM_ENABLED=0
./zork-native game/zork1.z3
```

Or simply don't set any `ZORK_LLM_*` variables.

## Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `ZORK_LLM_MOCK` | `0` | Set to `1` to enable mock mode (first 4 screens) |
| `ZORK_LLM_ENABLED` | `1` | Set to `0` to disable LLM completely |
| `ZORK_LLM_URL` | `http://localhost:1234/v1/chat/completions` | OpenAI-compatible API endpoint |
| `ZORK_LLM_MODEL` | `zork-assistant` | Model name to use |
| `ZORK_LLM_API_KEY` | *(none)* | API key for remote servers (optional) |

### Prompt Customization

The system prompts are stored in `prompts/` directory:

- `prompts/system.txt` - Defines the LLM's role as a command translator
- `prompts/user_template.txt` - Template for user messages with context
- `prompts/README.md` - Prompt engineering guide

**To customize prompts:**
1. Edit the text files in `prompts/`
2. No recompilation needed - changes take effect on next run
3. See `prompts/README.md` for tips on prompt engineering

## Architecture

### How It Works

1. **User types natural language**: "I want to open the mailbox"
2. **Context is gathered**: Last 20 turns of conversation
3. **LLM is called**: System sends context + user input to LLM
4. **Translation received**: LLM returns "open mailbox"
5. **Display to user**: `[LLM → open mailbox]`
6. **Game executes**: Z-machine processes the command
7. **Result captured**: Output saved for next context

### Modules

- **prompt_loader**: Loads prompts from text files
- **context**: Maintains conversation history (circular buffer)
- **output_capture**: Intercepts game output non-intrusively
- **json_helper**: Minimal JSON builder/parser for API calls
- **llm_client**: HTTP client using libcurl
- **input_translator**: Orchestrates all modules

### Graceful Degradation

If anything fails, the game remains playable:
- LLM server down? → Pass through original input
- Network timeout? → Pass through original input
- Invalid JSON? → Pass through original input
- Context error? → Continue without context

You'll never be blocked from playing!

## Educational Features

This is an **educational project** with extensive documentation:

- Every source file has detailed comments explaining concepts
- Prompts are editable text files (no hardcoded strings)
- Architecture uses clear design patterns (Facade, Observer)
- Statistics tracking shows LLM effectiveness
- Mock mode enables testing without infrastructure

## Statistics

When you quit, you'll see translation statistics:

```
=== Translation Statistics ===
Total attempts:  15
Successful:      14
Fallbacks:       1
Success rate:    93%
==============================
```

This helps you understand:
- How often the LLM was used
- How often it fell back
- Overall system reliability

## Troubleshooting

### "LLM client: Disabled via ZORK_LLM_ENABLED=0"

**Solution**: Unset the variable or set to `1`:
```bash
unset ZORK_LLM_ENABLED
# or
export ZORK_LLM_ENABLED=1
```

### "Failed to initialize libcurl"

**Cause**: libcurl not installed

**Solution**:
- macOS: `brew install curl`
- Ubuntu: `sudo apt-get install libcurl4-openssl-dev`
- Then rebuild: `./scripts/build_local.sh clean && ./scripts/build_local.sh debug`

### "Connection refused"

**Cause**: LLM server not running

**Solutions**:
1. Start your LLM server (e.g., LM Studio)
2. Use mock mode: `export ZORK_LLM_MOCK=1`
3. Disable LLM: `export ZORK_LLM_ENABLED=0`

### "Fallbacks: 100%"

**Cause**: LLM server not reachable or returning errors

**Debug**:
1. Check server is running: `curl http://localhost:1234/v1/models`
2. Verify URL: `echo $ZORK_LLM_URL`
3. Check server logs for errors
4. Try mock mode to verify integration works

## Performance

### Context Size

- Tracks last **20 turns** (configurable in `context.h`)
- Each turn: ~2KB (output + input + commands)
- Total context buffer: ~40KB
- Circular buffer prevents unbounded growth

### API Call Overhead

- Typical latency: 100-500ms (local LLM)
- Remote LLM: 500-2000ms
- Timeout: 5 seconds (falls back if exceeded)
- No blocking - game stays responsive

## Next Steps

### For Students

1. Read `src/llm/*.c` files - heavily commented
2. Experiment with prompts in `prompts/`
3. Try modifying context size (see `context.h`)
4. Trace an API call through all modules

### For Developers

1. Fine-tune a model specifically for Zork commands
2. Implement streaming responses (update as LLM generates)
3. Add multi-turn dialogue ("What do you mean by 'go'?")
4. Optimize context (truncate old/redundant turns)

### For RISC-V Deployment

Currently **native only** (Phase 2). For Phase 3:
- Option 1: Cross-compile libcurl for RISC-V
- Option 2: Implement minimal HTTP client
- Option 3: Use TT-Metal host communication
- Option 4: Run inference on Tensix cores (future)

## Examples

### Example 1: Natural Exploration

```
> I want to look around
[LLM → look]
West of House
You are standing in an open field...

> What's in the mailbox?
[LLM → open mailbox]
Opening the small mailbox reveals a leaflet.

> Pick it up and read it
[LLM → take leaflet, read leaflet]
Taken.
"WELCOME TO ZORK! ..."
```

### Example 2: Complex Actions

```
> I'd like to go north, open the window, and climb inside
[LLM → north, open window, enter house]
North of House
The windows are boarded and can't be opened.
I can't see how to get in from here.
```

### Example 3: Fallback

```
> xyzzy
[LLM translation failed: Network timeout]
A hollow voice says "Fool."
```

(Game continues normally with direct input)

## Contributing

Want to improve the LLM integration? Areas of interest:

1. **Prompt Engineering**: Better system prompts in `prompts/`
2. **Context Optimization**: Smarter history summarization
3. **Error Recovery**: Better handling of ambiguous translations
4. **Model Training**: Fine-tuning pipeline for Zork commands
5. **Multi-modal**: Add image generation for locations
6. **Voice Input**: Integrate speech-to-text

See `CLAUDE.md` for project architecture and history.

## License

Same as Frotz (GPL) - see parent README for details.
