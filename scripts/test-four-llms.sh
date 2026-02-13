#!/bin/bash
# Test all 4 LLM endpoints independently

echo "🧪 Testing 4-LLM Architecture Endpoints"
echo ""

# Function to get model name from a server
get_model_name() {
  local port=$1
  curl -s --max-time 2 http://localhost:$port/v1/models 2>/dev/null | jq -r '.data[0].id' 2>/dev/null
}

# Test function
test_llm() {
  local name=$1
  local port=$2
  local prompt=$3

  echo "[$name] Testing on port $port..."

  # Health check
  if ! curl -s --max-time 2 http://localhost:$port/health > /dev/null 2>&1; then
    echo "  ❌ Server not responding"
    return 1
  fi

  echo "  ✓ Health check passed"

  # Get actual model name
  model_name=$(get_model_name $port)
  if [ -z "$model_name" ] || [ "$model_name" = "null" ]; then
    echo "  ❌ Could not determine model name"
    return 1
  fi

  # Actual inference request
  response=$(curl -s --max-time 10 http://localhost:$port/v1/chat/completions \
    -H "Content-Type: application/json" \
    -d "{
      \"model\": \"$model_name\",
      \"messages\": [{\"role\": \"user\", \"content\": \"$prompt\"}],
      \"max_tokens\": 50,
      \"temperature\": 0.7
    }" 2>&1)

  if echo "$response" | grep -q "choices"; then
    echo "  ✓ Inference successful"
    # Extract and show first 100 chars of response
    content=$(echo "$response" | python3 -c "import sys, json; print(json.load(sys.stdin)['choices'][0]['message']['content'][:100])" 2>/dev/null || echo "")
    if [ -n "$content" ]; then
      echo "  Response: $content..."
    fi
    return 0
  else
    echo "  ❌ Inference failed"
    echo "  Error: $response" | head -3
    return 1
  fi
}

# Test each endpoint
echo "Testing LLM endpoints..."
echo ""

test_llm "Command Translator" 8000 "open the mailbox"
echo ""

test_llm "ASCII Artist" 8001 "Draw a simple house"
echo ""

test_llm "Dungeon Master" 8002 "Add atmosphere to: You are in a dark forest"
echo ""

test_llm "AI Player" 8003 "What should I do if I'm west of a house with a mailbox?"
echo ""

echo "✅ Testing complete!"
echo ""
echo "If all tests passed, you can now run Zork with:"
echo "  ./scripts/play-zork-four-llms.sh"
echo ""
