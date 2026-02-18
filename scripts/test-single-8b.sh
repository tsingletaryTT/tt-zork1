#!/bin/bash
# Quick test script for single 8B model setup
# Verifies that the model responds correctly to task-prefixed inputs

set -e

echo "🧪 Testing Single 8B Model Setup"
echo ""

# Check if server is running
if ! curl -s --max-time 2 http://localhost:9000/health > /dev/null 2>&1; then
    echo "❌ 8B model not running on port 9000"
    echo ""
    echo "Start it with: ./scripts/start-single-8b.sh"
    exit 1
fi

echo "✅ 8B model responding on port 9000"
echo ""

# Test each role
echo "Testing TRANSLATOR role..."
curl -s -X POST http://localhost:9000/v1/chat/completions \
    -H "Content-Type: application/json" \
    -d '{
        "model": "meta-llama/Llama-3.1-8B-Instruct",
        "messages": [
            {"role": "system", "content": "You are a command translator."},
            {"role": "user", "content": "[TRANSLATE] I want to go north"}
        ],
        "max_tokens": 50,
        "temperature": 0.1
    }' | jq -r '.choices[0].message.content' || echo "Failed"

echo ""
echo "Testing ARTIST role..."
curl -s -X POST http://localhost:9000/v1/chat/completions \
    -H "Content-Type: application/json" \
    -d '{
        "model": "meta-llama/Llama-3.1-8B-Instruct",
        "messages": [
            {"role": "system", "content": "You are an ASCII artist."},
            {"role": "user", "content": "[VISUALIZE] Forest"}
        ],
        "max_tokens": 150,
        "temperature": 0.5
    }' | jq -r '.choices[0].message.content' || echo "Failed"

echo ""
echo "Testing PLAYER role..."
curl -s -X POST http://localhost:9000/v1/chat/completions \
    -H "Content-Type: application/json" \
    -d '{
        "model": "meta-llama/Llama-3.1-8B-Instruct",
        "messages": [
            {"role": "system", "content": "You are a game player."},
            {"role": "user", "content": "[AUTOPLAY] You are west of a white house. Inventory: empty."}
        ],
        "max_tokens": 100,
        "temperature": 0.6
    }' | jq -r '.choices[0].message.content' || echo "Failed"

echo ""
echo "✅ All roles tested!"
echo ""
echo "Next: Run full A/B comparison with ./scripts/compare-setups.sh"
