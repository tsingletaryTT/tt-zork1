#!/bin/bash
# Quick translator testing with different configurations

PORT=8000
MODEL_NAME=$(curl -s --max-time 2 http://localhost:$PORT/v1/models 2>/dev/null | jq -r '.data[0].id' 2>/dev/null)

if [ -z "$MODEL_NAME" ]; then
  echo "ERROR: Could not get model name"
  exit 1
fi

echo "Testing Command Translator on Port $PORT"
echo "Model: $MODEL_NAME"
echo ""

# Test inputs
TESTS=(
  "open the mailbox"
  "pick up the brass lamp"
  "go north"
  "read the leaflet"
  "put the sword in the bag"
)

test_config() {
  local name=$1
  local prompt_file=$2
  local max_tokens=$3
  local temperature=$4

  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo "Configuration: $name"
  echo "  Prompt: $prompt_file"
  echo "  Max Tokens: $max_tokens"
  echo "  Temperature: $temperature"
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo ""

  # Load prompt
  SYSTEM_PROMPT=$(cat "$prompt_file" 2>/dev/null)
  if [ -z "$SYSTEM_PROMPT" ]; then
    echo "ERROR: Could not load $prompt_file"
    return
  fi

  for test_input in "${TESTS[@]}"; do
    echo "Input: \"$test_input\""

    START=$(date +%s%3N)
    RESPONSE=$(curl -s --max-time 10 http://localhost:$PORT/v1/chat/completions \
      -H "Content-Type: application/json" \
      -d "{
        \"model\": \"$MODEL_NAME\",
        \"messages\": [
          {\"role\": \"system\", \"content\": $(echo "$SYSTEM_PROMPT" | jq -Rs .)},
          {\"role\": \"user\", \"content\": $(echo "$test_input" | jq -Rs .)}
        ],
        \"max_tokens\": $max_tokens,
        \"temperature\": $temperature
      }")
    END=$(date +%s%3N)
    LATENCY=$((END - START))

    # Extract response
    CONTENT=$(echo "$RESPONSE" | jq -r '.choices[0].message.content' 2>/dev/null)

    # Try to extract just the command
    COMMAND=$(echo "$CONTENT" | sed 's/.*<\/think>//g' | grep -v '^[[:space:]]*$' | head -1 | sed 's/^[[:space:]]*//g;s/[[:space:]]*$//g')

    echo "  Full output:"
    echo "$CONTENT" | head -5
    if [ $(echo "$CONTENT" | wc -l) -gt 5 ]; then
      echo "  ... (truncated)"
    fi
    echo ""
    echo "  Extracted command: \"$COMMAND\""
    echo "  Latency: ${LATENCY}ms"
    echo ""
  done

  echo ""
}

# Test different configurations
if [ "$1" = "minimal" ]; then
  test_config "Minimal (50 tokens, temp 0.1)" "prompts/translator/system_v1_minimal.txt" 50 0.1
elif [ "$1" = "detailed" ]; then
  test_config "Detailed (100 tokens, temp 0.2)" "prompts/translator/system_v2_detailed.txt" 100 0.2
elif [ "$1" = "fewshot" ]; then
  test_config "Fewshot (75 tokens, temp 0.15)" "prompts/translator/system_v3_fewshot.txt" 75 0.15
elif [ "$1" = "fewshot-more-tokens" ]; then
  test_config "Fewshot + More Tokens (150 tokens, temp 0.15)" "prompts/translator/system_v3_fewshot.txt" 150 0.15
elif [ "$1" = "fewshot-lower-temp" ]; then
  test_config "Fewshot + Lower Temp (150 tokens, temp 0.05)" "prompts/translator/system_v3_fewshot.txt" 150 0.05
else
  echo "Usage: $0 <config>"
  echo ""
  echo "Available configs:"
  echo "  minimal              - Ultra-concise (50 tokens, temp 0.1)"
  echo "  detailed             - More examples (100 tokens, temp 0.2)"
  echo "  fewshot              - Pattern-based (75 tokens, temp 0.15) [current]"
  echo "  fewshot-more-tokens  - Pattern-based + more tokens (150, temp 0.15)"
  echo "  fewshot-lower-temp   - Pattern-based + lower temp (150, temp 0.05)"
  echo ""
fi
