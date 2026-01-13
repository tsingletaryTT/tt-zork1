#!/usr/bin/env bash
#
# run-zork-llm.sh - Run Zork with LLM natural language translation
#
# This script configures and runs Zork with Ollama + Qwen2.5:1.5b model
# for natural language command translation.

# Ollama configuration
export ZORK_LLM_URL="http://localhost:11434/v1/chat/completions"
export ZORK_LLM_MODEL="${ZORK_LLM_MODEL:-qwen2.5:1.5b}"  # Default to 1.5B, override with env var
export ZORK_LLM_ENABLED="1"

# Check if Ollama is running
if ! curl -s http://localhost:11434/api/version > /dev/null 2>&1; then
    echo "Error: Ollama server not running"
    echo "Start it with: ollama serve"
    echo ""
    echo "Or use mock mode: export ZORK_LLM_MOCK=1 && ./zork-native game/zork1.z3"
    exit 1
fi

# Check if model is available
if ! ollama list | grep -q "$ZORK_LLM_MODEL"; then
    echo "Error: $ZORK_LLM_MODEL model not found"
    echo "Pull it with: ollama pull $ZORK_LLM_MODEL"
    echo ""
    echo "Recommended models:"
    echo "  - qwen2.5:0.5b (397MB)  - Fast, good for direct commands"
    echo "  - qwen2.5:1.5b (986MB)  - RECOMMENDED: Best natural language"
    echo "  - qwen2.5:3b (1.9GB)    - Even better, if you have RAM"
    exit 1
fi

echo "=================================================="
echo "  🎮 Zork with LLM Natural Language Translation"
echo "=================================================="
echo ""
echo "Configuration:"
echo "  LLM Server: Ollama (localhost:11434)"
echo "  Model: $ZORK_LLM_MODEL"
echo "  Mode: Real-time translation"
echo ""
echo "Tips:"
echo "  - Use natural language: 'I want to open the mailbox'"
echo "  - The LLM will translate to: 'open mailbox'"
echo "  - You'll see both your input and the translation"
echo ""
echo "Starting game..."
echo ""

./zork-native game/zork1.z3
