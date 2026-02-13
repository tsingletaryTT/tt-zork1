#!/bin/bash
# Test different prompt variants to find optimal configuration

set -e

RESULTS_DIR="experiments/prompt_variants_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS_DIR"

echo "🧪 Prompt Variant Testing Framework"
echo ""
echo "Results will be saved to: $RESULTS_DIR"
echo ""

# Test cases for each LLM
declare -A TRANSLATOR_TESTS=(
  ["casual"]="I want to open the mailbox"
  ["polite"]="Could you please pick up the lamp?"
  ["complex"]="Go north and then take the sword"
  ["ambiguous"]="open it"
  ["verbose"]="I would like to examine the beautiful painting on the wall"
)

declare -A ARTIST_TESTS=(
  ["west_house"]="West of House|Open field with white house|white house,mailbox,forest|peaceful"
  ["cellar"]="Cellar|Dark underground room|stairs,darkness|ominous"
  ["forest"]="Forest|Dense woods|trees,path|mysterious"
)

declare -A DM_TESTS=(
  ["routine"]="West of House|You are standing in an open field west of a white house.|none"
  ["discovery"]="Mailbox|Opening the small mailbox reveals a leaflet.|opened mailbox"
  ["danger"]="Dark Place|It is pitch black. You are likely to be eaten by a grue.|entered darkness"
)

# Test translator variants
test_translator() {
  local variant=$1
  local prompt_file=$2
  local max_tokens=$3
  local temperature=$4

  # Get model name
  local model_name=$(curl -s --max-time 2 http://localhost:8000/v1/models 2>/dev/null | jq -r '.data[0].id' 2>/dev/null)
  if [ -z "$model_name" ] || [ "$model_name" = "null" ]; then
    echo "  ERROR: Could not get model name from port 8000"
    return 1
  fi

  echo "Testing Translator variant: $variant"
  echo "  Prompt: $prompt_file"
  echo "  Max tokens: $max_tokens, Temperature: $temperature"
  echo "  Model: $model_name"
  echo ""

  local results_file="$RESULTS_DIR/translator_${variant}.txt"
  echo "=== TRANSLATOR VARIANT: $variant ===" > "$results_file"
  echo "Prompt file: $prompt_file" >> "$results_file"
  echo "Parameters: max_tokens=$max_tokens, temperature=$temperature" >> "$results_file"
  echo "" >> "$results_file"

  for test_name in "${!TRANSLATOR_TESTS[@]}"; do
    local input="${TRANSLATOR_TESTS[$test_name]}"
    echo "  Test: $test_name"
    echo "    Input: $input"

    # Load system prompt
    local system_prompt=$(cat "prompts/$prompt_file")

    # Make API request
    local start_time=$(date +%s%3N)
    local response=$(curl -s --max-time 10 http://localhost:8000/v1/chat/completions \
      -H "Content-Type: application/json" \
      -d "{
        \"model\": \"$model_name\",
        \"messages\": [
          {\"role\": \"system\", \"content\": $(echo "$system_prompt" | jq -Rs .)},
          {\"role\": \"user\", \"content\": $(echo "$input" | jq -Rs .)}
        ],
        \"max_tokens\": $max_tokens,
        \"temperature\": $temperature
      }" 2>&1)
    local end_time=$(date +%s%3N)
    local latency=$((end_time - start_time))

    # Extract response
    local output=$(echo "$response" | jq -r '.choices[0].message.content' 2>/dev/null || echo "ERROR")

    echo "    Output: $output"
    echo "    Latency: ${latency}ms"
    echo ""

    # Log to results file
    echo "Test: $test_name" >> "$results_file"
    echo "Input: $input" >> "$results_file"
    echo "Output: $output" >> "$results_file"
    echo "Latency: ${latency}ms" >> "$results_file"
    echo "" >> "$results_file"
  done

  echo ""
}

# Test ASCII artist variants
test_artist() {
  local variant=$1
  local prompt_file=$2
  local max_tokens=$3
  local temperature=$4

  # Get model name
  local model_name=$(curl -s --max-time 2 http://localhost:8001/v1/models 2>/dev/null | jq -r '.data[0].id' 2>/dev/null)
  if [ -z "$model_name" ] || [ "$model_name" = "null" ]; then
    echo "  ERROR: Could not get model name from port 8001"
    return 1
  fi

  echo "Testing Artist variant: $variant"
  echo "  Prompt: $prompt_file"
  echo "  Max tokens: $max_tokens, Temperature: $temperature"
  echo "  Model: $model_name"
  echo ""

  local results_file="$RESULTS_DIR/artist_${variant}.txt"
  echo "=== ARTIST VARIANT: $variant ===" > "$results_file"
  echo "Prompt file: $prompt_file" >> "$results_file"
  echo "Parameters: max_tokens=$max_tokens, temperature=$temperature" >> "$results_file"
  echo "" >> "$results_file"

  for test_name in "${!ARTIST_TESTS[@]}"; do
    local test_data="${ARTIST_TESTS[$test_name]}"
    IFS='|' read -r location description objects mood <<< "$test_data"

    echo "  Test: $test_name"
    echo "    Location: $location"

    # Load and customize system prompt
    local system_prompt=$(cat "prompts/$prompt_file")
    system_prompt="${system_prompt//\{LOCATION\}/$location}"
    system_prompt="${system_prompt//\{DESCRIPTION\}/$description}"
    system_prompt="${system_prompt//\{OBJECTS\}/$objects}"
    system_prompt="${system_prompt//\{MOOD\}/$mood}"

    # Make API request
    local start_time=$(date +%s%3N)
    local response=$(curl -s --max-time 15 http://localhost:8001/v1/chat/completions \
      -H "Content-Type: application/json" \
      -d "{
        \"model\": \"$model_name\",
        \"messages\": [
          {\"role\": \"system\", \"content\": $(echo "$system_prompt" | jq -Rs .)},
          {\"role\": \"user\", \"content\": \"Generate ASCII art\"}
        ],
        \"max_tokens\": $max_tokens,
        \"temperature\": $temperature
      }" 2>&1)
    local end_time=$(date +%s%3N)
    local latency=$((end_time - start_time))

    # Extract response
    local output=$(echo "$response" | jq -r '.choices[0].message.content' 2>/dev/null || echo "ERROR")

    echo "    Latency: ${latency}ms"
    echo ""

    # Log to results file
    echo "Test: $test_name" >> "$results_file"
    echo "Location: $location" >> "$results_file"
    echo "Output:" >> "$results_file"
    echo "$output" >> "$results_file"
    echo "Latency: ${latency}ms" >> "$results_file"
    echo "" >> "$results_file"
  done

  echo ""
}

# Main test execution
echo "Starting prompt variant tests..."
echo ""

# Test translator variants
if curl -s --max-time 2 http://localhost:8000/health > /dev/null 2>&1; then
  echo "=== TESTING TRANSLATOR VARIANTS ==="
  echo ""

  test_translator "minimal" "translator/system_v1_minimal.txt" 50 0.1
  test_translator "detailed" "translator/system_v2_detailed.txt" 100 0.2
  test_translator "fewshot" "translator/system_v3_fewshot.txt" 75 0.15
else
  echo "⚠️  Translator endpoint (port 8000) not available, skipping"
  echo ""
fi

# Test artist variants
if curl -s --max-time 2 http://localhost:8001/health > /dev/null 2>&1; then
  echo "=== TESTING ARTIST VARIANTS ==="
  echo ""

  test_artist "simple" "artist/system_v1_simple.txt" 300 0.7
  test_artist "atmospheric" "artist/system_v2_atmospheric.txt" 400 0.8
else
  echo "⚠️  Artist endpoint (port 8001) not available, skipping"
  echo ""
fi

# Generate summary report
echo "=== GENERATING SUMMARY REPORT ==="
echo ""

cat > "$RESULTS_DIR/README.md" << EOF
# Prompt Variant Testing Results

**Date**: $(date)
**Experiment ID**: $(basename $RESULTS_DIR)

## Test Configuration

### Translator Tests
- minimal: Ultra-concise, fast (50 tokens, temp 0.1)
- detailed: More examples (100 tokens, temp 0.2)
- fewshot: Pattern-based (75 tokens, temp 0.15)

### Artist Tests
- simple: Clean minimal (300 tokens, temp 0.7)
- atmospheric: Detailed moody (400 tokens, temp 0.8)

## Results Files
- \`translator_*.txt\` - Translation test outputs
- \`artist_*.txt\` - ASCII art generations

## How to Evaluate

### Translator
1. Check accuracy: Does output match expected Zork command?
2. Check latency: Is it under 200ms target?
3. Check consistency: Same input → same output?

### Artist
1. Check quality: Is the ASCII art recognizable?
2. Check formatting: Does it fit within bounds?
3. Check atmosphere: Does it match the mood?

## Recommendation Template

Based on testing:
- **Translator**: Use [variant] because [reason]
- **Artist**: Use [variant] because [reason]

Update \`prompts/config.yaml\` with recommendations.
EOF

echo "✅ Testing complete!"
echo ""
echo "Results saved to: $RESULTS_DIR"
echo ""
echo "Review results and update config.yaml with your findings."
echo ""
