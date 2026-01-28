# Ollama Integration - Qwen2.5:0.5b

**Date**: January 12, 2026
**Status**: ✅ **WORKING**

## Summary

Successfully integrated Ollama with Qwen2.5:0.5b model for real-time natural language translation in Zork. The system translates conversational English into classic text adventure commands.

## Setup

### 1. Install Ollama (if not already installed)

```bash
brew install ollama  # macOS
# Ollama starts automatically as a service
```

### 2. Pull the Qwen2.5:0.5b model

```bash
ollama pull qwen2.5:0.5b
# Downloads ~397MB model
```

### 3. Verify Ollama is running

```bash
curl http://localhost:11434/api/version
# Should return version info
```

## Running Zork with LLM

### Quick Start

```bash
./run-zork-llm.sh
```

This script automatically:
- Checks if Ollama is running
- Verifies the model is available
- Configures environment variables
- Launches Zork with LLM enabled

### Manual Configuration

```bash
export ZORK_LLM_URL="http://localhost:11434/v1/chat/completions"
export ZORK_LLM_MODEL="qwen2.5:0.5b"
export ZORK_LLM_ENABLED="1"

./zork-native game/zork1.z3
```

## Demo

```bash
./demo-llm.sh
```

Shows natural language translation in action with pre-scripted inputs.

## Example Session

```
> I'd like to check what's in the mailbox
[LLM → open mailbox]
Opening the small mailbox reveals a leaflet.

> Please take the leaflet
[LLM → take leaflet]
Taken.

> Can you read the leaflet for me?
[LLM → read leaflet]
"WELCOME TO ZORK! ..."

> Let's go to the north
[LLM → north]
North of House
You are facing the north side of a white house...
```

## Technical Details

### Model Information

- **Name**: Qwen2.5:0.5b
- **Size**: 397MB (0.5 billion parameters)
- **Type**: Instruction-tuned language model
- **Performance**: Fast inference (~100-500ms per translation on Mac)
- **Quality**: Good for basic commands, may add extra context sometimes

### API Endpoint

Ollama provides an OpenAI-compatible API:
- **URL**: `http://localhost:11434/v1/chat/completions`
- **Format**: Standard OpenAI chat completion format
- **Authentication**: None required for local instance

### Integration Architecture

```
User Input: "I want to open the mailbox"
    ↓
Context Manager (last 20 turns)
    ↓
Prompt Loader (system + user prompts)
    ↓
JSON Builder (OpenAI format)
    ↓
HTTP POST → http://localhost:11434/v1/chat/completions
    ↓
Ollama → Qwen2.5:0.5b
    ↓
Response: "open mailbox"
    ↓
Display: [LLM → open mailbox]
    ↓
Z-machine executes command
```

## Translation Quality

### Strengths
- ✅ Simple commands translated accurately
- ✅ Natural language understanding works well
- ✅ Fast inference (sub-second)
- ✅ Runs locally (privacy, no API costs)

### Limitations
- ⚠️ Sometimes adds extra commands
- ⚠️ May include formatting artifacts (e.g., "exit<")
- ⚠️ 0.5B model not fine-tuned for Zork specifically
- ⚠️ Context not always used optimally

### Example Translations

| Natural Language | LLM Translation | Result |
|-----------------|-----------------|---------|
| "I want to open the mailbox" | `open mailbox` | ✅ Perfect |
| "Please take the leaflet" | `take leaflet` | ✅ Good |
| "Can you read it?" | `read leaflet` | ✅ Good |
| "Let's go north" | `north` | ✅ Perfect |
| "Go through the white door" | `north, open door` | ⚠️ Extra command |

## Improvements

### For Better Translations

1. **Fine-tune the model** on Zork-specific commands
   - Create training data from gameplay sessions
   - Fine-tune Qwen2.5:0.5b on command pairs

2. **Optimize prompts** (no recompilation needed!)
   - Edit `prompts/system.txt`
   - Add more examples in system prompt
   - Adjust temperature (lower = more conservative)

3. **Use a larger model**
   - Try `qwen2.5:1.5b` or `qwen2.5:3b`
   - Trade-off: slower inference, better quality

4. **Improve context**
   - Adjust context size in `src/llm/context.h`
   - Better context formatting in prompts

## Performance

### Benchmarks (Apple Silicon Mac)

- **Model load time**: ~2 seconds (first request)
- **Average translation**: 200-400ms
- **Context overhead**: Minimal (~40KB per 20 turns)
- **Memory usage**: ~500MB (Ollama + model)

### Optimization

Ollama automatically:
- Keeps model in memory after first use
- Uses Metal GPU acceleration on macOS
- Caches common token patterns

## Troubleshooting

### "Connection refused"

```bash
# Check if Ollama is running
curl http://localhost:11434/api/version

# If not, start it
ollama serve
```

### "Model not found"

```bash
# Pull the model
ollama pull qwen2.5:0.5b

# Verify it's available
ollama list | grep qwen
```

### "Translations are poor"

1. Try a larger model: `ollama pull qwen2.5:1.5b`
2. Edit prompts in `prompts/system.txt`
3. Lower temperature in `src/llm/llm_client.c` (line 242)

### "Too slow"

1. Ensure Ollama is using GPU: Check Activity Monitor
2. Try smaller model: Already using smallest (0.5b)
3. Reduce max_tokens in client config

## Comparison to Mock Mode

| Feature | Mock Mode | Ollama Mode |
|---------|-----------|-------------|
| Setup | None needed | Ollama + model |
| Speed | Instant | ~200-400ms |
| Quality | Canned responses | Dynamic translation |
| Coverage | First 4 inputs | Unlimited |
| Context-aware | No | Yes |
| Cost | Free | Free (local) |

## Future Enhancements

### Short Term
- [ ] Fine-tune Qwen2.5:0.5b on Zork corpus
- [ ] Add more examples to system prompt
- [ ] Implement retry logic for failed translations
- [ ] Cache common translations

### Medium Term
- [ ] Support for multi-turn dialogue with LLM
- [ ] Streaming responses (show translation as it generates)
- [ ] Voice input → Whisper → LLM → Zork
- [ ] Support for other Infocom games

### Long Term
- [ ] RISC-V port with Ollama on host
- [ ] On-device inference with Tensix cores
- [ ] LLM-to-LLM gameplay (autonomous agents)
- [ ] Visual generation for room descriptions

## Conclusion

The Ollama + Qwen2.5:0.5b integration demonstrates that local LLM inference can power natural language interfaces for classic games. The system is:

- ✅ **Working** - Translates natural language to commands
- ✅ **Fast** - Sub-second inference
- ✅ **Local** - No cloud dependencies
- ✅ **Educational** - All code heavily documented
- ✅ **Extensible** - Prompts editable without recompilation

**Ready for**: Extended gameplay testing, prompt optimization, fine-tuning experiments.

---

**Files**:
- `run-zork-llm.sh` - Launch script
- `demo-llm.sh` - Demo script
- `prompts/system.txt` - Editable system prompt
- `docs/LLM_USAGE.md` - Complete usage guide
