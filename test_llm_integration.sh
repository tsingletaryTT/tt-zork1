#!/bin/bash
# Test LLM Integration - Verify all 4 agents are working

echo "=== Testing LLM Integration ==="
echo ""

# Check if all 4 LLM servers are running
echo "1. Checking LLM servers..."
echo ""

for port in 8000 8001 8002 8003; do
    chip=$((port - 8000))
    model=$(curl -s http://localhost:$port/v1/models 2>/dev/null | jq -r '.data[0].id // "NOT RUNNING"')
    if [ "$model" != "NOT RUNNING" ]; then
        echo "  ✓ Port $port (Chip $chip): $model"
    else
        echo "  ✗ Port $port (Chip $chip): NOT RUNNING"
        echo "      Start with: tt serve start <model> --device-id $chip --port $port"
    fi
done
echo ""

# Check config file
echo "2. Checking configuration..."
if [ -f "config/llm_mode.yaml" ]; then
    mode=$(grep "^mode:" config/llm_mode.yaml | awk '{print $2}')
    echo "  ✓ Config file exists"
    echo "  ✓ Mode: $mode"
else
    echo "  ✗ Config file missing!"
fi
echo ""

# Test translator (Chip 0)
echo "3. Testing Translator (Chip 0, port 8000)..."
response=$(curl -s -X POST http://localhost:8000/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "Qwen3-0.6B",
    "messages": [
      {"role": "system", "content": "Translate natural language to Zork commands. Output ONLY the command."},
      {"role": "user", "content": "I want to go north"}
    ],
    "max_tokens": 20,
    "temperature": 0.1
  }' 2>/dev/null)

if echo "$response" | jq -e '.choices[0].message.content' > /dev/null 2>&1; then
    command=$(echo "$response" | jq -r '.choices[0].message.content')
    echo "  ✓ Translator working: '$command'"
else
    echo "  ✗ Translator failed: $response"
fi
echo ""

# Test artist (Chip 1)
echo "4. Testing Artist (Chip 1, port 8001)..."
response=$(curl -s -X POST http://localhost:8001/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "Llama-3.2-1B-Instruct",
    "messages": [
      {"role": "system", "content": "Generate ASCII art for game locations."},
      {"role": "user", "content": "West of House"}
    ],
    "max_tokens": 50,
    "temperature": 0.5
  }' 2>/dev/null)

if echo "$response" | jq -e '.choices[0].message.content' > /dev/null 2>&1; then
    echo "  ✓ Artist working"
else
    echo "  ✗ Artist failed: $response"
fi
echo ""

# Test Player agent (Chip 3)
echo "5. Testing Player Agent (Chip 3, port 8003)..."
response=$(curl -s -X POST http://localhost:8003/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "Qwen3-0.6B",
    "messages": [
      {"role": "system", "content": "You are an expert Zork player. Output ONLY a single command."},
      {"role": "user", "content": "Game: West of House. Small mailbox here.\nWhat do you do?"}
    ],
    "max_tokens": 20,
    "temperature": 0.6
  }' 2>/dev/null)

if echo "$response" | jq -e '.choices[0].message.content' > /dev/null 2>&1; then
    command=$(echo "$response" | jq -r '.choices[0].message.content')
    echo "  ✓ Player agent working: '$command'"
else
    echo "  ✗ Player agent failed: $response"
fi
echo ""

echo "=== Interactive Test ==="
echo ""
echo "To test the full integration:"
echo ""
echo "1. Test natural language translation:"
echo "   $ ./zork-native game/zork1.z3"
echo "   > I want to open the mailbox"
echo "   (Should translate to 'open mailbox')"
echo ""
echo "2. Test slash commands:"
echo "   > /help"
echo "   > /status"
echo "   > /mode enhanced"
echo ""
echo "3. Test auto-play mode:"
echo "   > /play auto"
echo "   > /player naive"
echo "   (AI should start playing automatically)"
echo ""
echo "4. Test ASCII art:"
echo "   (Should appear after location descriptions if artist is working)"
echo ""

# Check if prompt files exist
echo "=== Checking Prompt Files ==="
echo ""
for file in prompts/translator/system_v5_magic.txt \
            prompts/artist/system_v9_magic.txt \
            prompts/dm/system_v5_magic.txt \
            prompts/player/system_v3_magic.txt; do
    if [ -f "$file" ]; then
        echo "  ✓ $file"
    else
        echo "  ✗ $file MISSING"
    fi
done
echo ""

echo "=== Summary ==="
echo ""
echo "If all tests passed:"
echo "  • Natural language translation should work"
echo "  • ASCII art should appear (if enabled)"
echo "  • Auto-play mode should work with /play auto"
echo "  • Persona switching should work with /player <persona>"
echo ""
echo "If tests failed:"
echo "  • Check that all 4 LLM servers are running"
echo "  • Check config/llm_mode.yaml is set to 'multi_agent' mode"
echo "  • Check prompt files exist in prompts/ directories"
echo "  • Try: tt serve start Qwen3-0.6B --device-id <N> --port <PORT>"
echo ""
