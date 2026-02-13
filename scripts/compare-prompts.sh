#!/bin/bash
# Side-by-side comparison of prompt variants

set -e

echo "📊 Prompt Variant Comparison Tool"
echo ""

if [ $# -lt 2 ]; then
  echo "Usage: $0 <llm> <test_input>"
  echo ""
  echo "Examples:"
  echo "  $0 translator \"open the mailbox\""
  echo "  $0 artist \"West of House\""
  echo ""
  exit 1
fi

LLM_TYPE=$1
TEST_INPUT=$2

# Configuration based on LLM type
case $LLM_TYPE in
  translator)
    PORT=8000
    VARIANTS=("minimal" "detailed" "fewshot")
    PROMPT_FILES=(
      "prompts/translator/system_v1_minimal.txt"
      "prompts/translator/system_v2_detailed.txt"
      "prompts/translator/system_v3_fewshot.txt"
    )
    MAX_TOKENS=100
    ;;
  artist)
    PORT=8001
    VARIANTS=("simple" "atmospheric")
    PROMPT_FILES=(
      "prompts/artist/system_v1_simple.txt"
      "prompts/artist/system_v2_atmospheric.txt"
    )
    MAX_TOKENS=400
    ;;
  dm)
    PORT=8002
    VARIANTS=("subtle" "dramatic")
    PROMPT_FILES=(
      "prompts/dm/system_v1_subtle.txt"
      "prompts/dm/system_v2_dramatic.txt"
    )
    MAX_TOKENS=300
    ;;
  *)
    echo "Invalid LLM type: $LLM_TYPE"
    echo "Valid types: translator, artist, dm"
    exit 1
    ;;
esac

# Check server
if ! curl -s --max-time 2 http://localhost:$PORT/health > /dev/null 2>&1; then
  echo "ERROR: Server not responding on port $PORT"
  exit 1
fi

echo "Testing: $TEST_INPUT"
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Test each variant
for i in "${!VARIANTS[@]}"; do
  variant="${VARIANTS[$i]}"
  prompt_file="${PROMPT_FILES[$i]}"

  echo "╔═══════════════════════════════════════════════════════╗"
  echo "║  VARIANT: $variant"
  echo "╚═══════════════════════════════════════════════════════╝"
  echo ""

  # Load system prompt
  system_prompt=$(cat "$prompt_file" 2>/dev/null || echo "ERROR: File not found")

  if [ "$system_prompt" = "ERROR: File not found" ]; then
    echo "⚠️  Could not load: $prompt_file"
    echo ""
    continue
  fi

  # Make request
  start=$(date +%s%3N)
  response=$(curl -s --max-time 10 http://localhost:$PORT/v1/chat/completions \
    -H "Content-Type: application/json" \
    -d "{
      \"model\": \"Qwen3-0.6B\",
      \"messages\": [
        {\"role\": \"system\", \"content\": $(echo "$system_prompt" | jq -Rs .)},
        {\"role\": \"user\", \"content\": $(echo "$TEST_INPUT" | jq -Rs .)}
      ],
      \"max_tokens\": $MAX_TOKENS,
      \"temperature\": 0.7
    }" 2>&1)
  end=$(date +%s%3N)
  latency=$((end - start))

  # Extract results
  output=$(echo "$response" | jq -r '.choices[0].message.content' 2>/dev/null || echo "ERROR")
  tokens=$(echo "$response" | jq -r '.usage.completion_tokens' 2>/dev/null || echo "?")

  # Display
  echo "Output:"
  echo "─────────────────────────────────────────────────────────"
  echo "$output"
  echo "─────────────────────────────────────────────────────────"
  echo ""
  echo "⏱️  Latency: ${latency}ms"
  echo "📝 Tokens: $tokens"
  echo "📄 Prompt: $prompt_file"
  echo ""
done

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "💡 Tips:"
echo "  - Compare accuracy, latency, and output quality"
echo "  - Test with multiple inputs to find patterns"
echo "  - Update config.yaml with your preferred variant"
echo ""
