#!/bin/bash
# Interactive prompt tuning tool
# Allows real-time experimentation with prompts and parameters

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🎛️  Interactive Prompt Tuning Tool${NC}"
echo ""
echo "Test prompts in real-time with different parameters."
echo ""

# Select LLM to tune
echo "Which LLM do you want to tune?"
echo "  1) Command Translator (port 8000)"
echo "  2) ASCII Artist (port 8001)"
echo "  3) Dungeon Master (port 8002)"
echo "  4) AI Player (port 8003)"
echo ""
read -p "Enter number: " llm_choice

case $llm_choice in
  1)
    LLM_NAME="Translator"
    PORT=8000
    DEFAULT_PROMPT="prompts/translator/system_v3_fewshot.txt"
    DEFAULT_MAX_TOKENS=75
    DEFAULT_TEMPERATURE=0.15
    ;;
  2)
    LLM_NAME="Artist"
    PORT=8001
    DEFAULT_PROMPT="prompts/artist/system_v2_atmospheric.txt"
    DEFAULT_MAX_TOKENS=400
    DEFAULT_TEMPERATURE=0.8
    ;;
  3)
    LLM_NAME="DM"
    PORT=8002
    DEFAULT_PROMPT="prompts/dm/system_v2_dramatic.txt"
    DEFAULT_MAX_TOKENS=300
    DEFAULT_TEMPERATURE=0.9
    ;;
  4)
    LLM_NAME="Player"
    PORT=8003
    DEFAULT_PROMPT="prompts/player/system_v2_strategic.txt"
    DEFAULT_MAX_TOKENS=400
    DEFAULT_TEMPERATURE=0.7
    ;;
  *)
    echo "Invalid choice"
    exit 1
    ;;
esac

echo ""
echo -e "${GREEN}Selected: $LLM_NAME (port $PORT)${NC}"
echo ""

# Check if server is running
if ! curl -s --max-time 2 http://localhost:$PORT/health > /dev/null 2>&1; then
  echo -e "${RED}ERROR: Server not responding on port $PORT${NC}"
  echo "Start servers with: ./scripts/start-four-llms.sh"
  exit 1
fi

# Get actual model name
MODEL_NAME=$(curl -s --max-time 2 http://localhost:$PORT/v1/models 2>/dev/null | jq -r '.data[0].id' 2>/dev/null)
if [ -z "$MODEL_NAME" ] || [ "$MODEL_NAME" = "null" ]; then
  echo -e "${RED}ERROR: Could not determine model name${NC}"
  exit 1
fi

echo "Model: $MODEL_NAME"
echo ""

# Load current settings
PROMPT_FILE=$DEFAULT_PROMPT
MAX_TOKENS=$DEFAULT_MAX_TOKENS
TEMPERATURE=$DEFAULT_TEMPERATURE
TOP_P=0.95

# Main interaction loop
while true; do
  echo ""
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo -e "${YELLOW}Current Configuration:${NC}"
  echo "  Prompt file: $PROMPT_FILE"
  echo "  Max tokens: $MAX_TOKENS"
  echo "  Temperature: $TEMPERATURE"
  echo "  Top-p: $TOP_P"
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo ""
  echo "Commands:"
  echo "  test <input>     - Test with input"
  echo "  prompt <file>    - Change prompt file"
  echo "  tokens <n>       - Set max tokens"
  echo "  temp <f>         - Set temperature (0.0-2.0)"
  echo "  topp <f>         - Set top-p (0.0-1.0)"
  echo "  save <name>      - Save current config as variant"
  echo "  compare          - Run comparison test"
  echo "  quit             - Exit"
  echo ""
  read -p "> " command args

  case "$command" in
    test)
      if [ -z "$args" ]; then
        echo "Usage: test <input>"
        continue
      fi

      echo ""
      echo -e "${BLUE}Testing with input: $args${NC}"

      # Load system prompt
      SYSTEM_PROMPT=$(cat "$PROMPT_FILE" 2>/dev/null || echo "")
      if [ -z "$SYSTEM_PROMPT" ]; then
        echo -e "${RED}ERROR: Could not load prompt file: $PROMPT_FILE${NC}"
        continue
      fi

      # Make API request
      START_TIME=$(date +%s%3N)
      RESPONSE=$(curl -s --max-time 10 http://localhost:$PORT/v1/chat/completions \
        -H "Content-Type: application/json" \
        -d "{
          \"model\": \"$MODEL_NAME\",
          \"messages\": [
            {\"role\": \"system\", \"content\": $(echo "$SYSTEM_PROMPT" | jq -Rs .)},
            {\"role\": \"user\", \"content\": $(echo "$args" | jq -Rs .)}
          ],
          \"max_tokens\": $MAX_TOKENS,
          \"temperature\": $TEMPERATURE,
          \"top_p\": $TOP_P
        }" 2>&1)
      END_TIME=$(date +%s%3N)
      LATENCY=$((END_TIME - START_TIME))

      # Extract response
      OUTPUT=$(echo "$RESPONSE" | jq -r '.choices[0].message.content' 2>/dev/null || echo "ERROR: Invalid response")
      TOKEN_COUNT=$(echo "$RESPONSE" | jq -r '.usage.completion_tokens' 2>/dev/null || echo "?")

      echo ""
      echo -e "${GREEN}━━━ OUTPUT ━━━${NC}"
      echo "$OUTPUT"
      echo -e "${GREEN}━━━━━━━━━━━━━${NC}"
      echo ""
      echo "Latency: ${LATENCY}ms"
      echo "Tokens used: $TOKEN_COUNT / $MAX_TOKENS"
      ;;

    prompt)
      if [ -z "$args" ]; then
        echo "Available prompts in prompts/$LLM_NAME/:"
        ls -1 prompts/$(echo $LLM_NAME | tr '[:upper:]' '[:lower:]')/*.txt 2>/dev/null | sed 's|prompts/||'
        continue
      fi

      if [ ! -f "prompts/$args" ] && [ ! -f "$args" ]; then
        echo -e "${RED}ERROR: File not found: $args${NC}"
        continue
      fi

      PROMPT_FILE="$args"
      echo -e "${GREEN}✓ Switched to: $PROMPT_FILE${NC}"
      ;;

    tokens)
      if [ -z "$args" ] || ! [[ "$args" =~ ^[0-9]+$ ]]; then
        echo "Usage: tokens <number>"
        continue
      fi

      MAX_TOKENS=$args
      echo -e "${GREEN}✓ Max tokens set to: $MAX_TOKENS${NC}"
      ;;

    temp)
      if [ -z "$args" ]; then
        echo "Usage: temp <0.0-2.0>"
        continue
      fi

      TEMPERATURE=$args
      echo -e "${GREEN}✓ Temperature set to: $TEMPERATURE${NC}"
      ;;

    topp)
      if [ -z "$args" ]; then
        echo "Usage: topp <0.0-1.0>"
        continue
      fi

      TOP_P=$args
      echo -e "${GREEN}✓ Top-p set to: $TOP_P${NC}"
      ;;

    save)
      if [ -z "$args" ]; then
        echo "Usage: save <variant_name>"
        continue
      fi

      VARIANT_FILE="experiments/configs/${args}.yaml"
      mkdir -p experiments/configs

      cat > "$VARIANT_FILE" << EOF
# Saved configuration: $args
# Date: $(date)
# LLM: $LLM_NAME

prompt_file: $PROMPT_FILE
max_tokens: $MAX_TOKENS
temperature: $TEMPERATURE
top_p: $TOP_P
EOF

      echo -e "${GREEN}✓ Configuration saved to: $VARIANT_FILE${NC}"
      ;;

    compare)
      echo ""
      echo "Running comparison test..."
      echo "Testing current config against all available prompt variants..."
      echo ""

      # TODO: Implement side-by-side comparison
      echo "Feature coming soon!"
      ;;

    quit|exit|q)
      echo "Goodbye!"
      exit 0
      ;;

    *)
      echo "Unknown command: $command"
      ;;
  esac
done
