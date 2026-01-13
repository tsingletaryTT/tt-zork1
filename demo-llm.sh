#!/usr/bin/env bash
#
# demo-llm.sh - Demonstrate LLM natural language translation
#
# Shows Zork responding to natural language inputs

export ZORK_LLM_URL="http://localhost:11434/v1/chat/completions"
export ZORK_LLM_MODEL="qwen2.5:0.5b"

echo "=================================================="
echo "  🎮 Zork LLM Demo - Natural Language Commands"
echo "=================================================="
echo ""
echo "Watch as natural language gets translated to Zork commands!"
echo ""

# Create input file with natural language commands
cat > /tmp/zork-demo-input.txt << 'EOF'
I'd like to check what's in the mailbox
Please take the leaflet
Can you read the leaflet for me?
Let's go to the north
I want to quit now
y
EOF

echo "Natural language inputs:"
echo "  1. 'I'd like to check what's in the mailbox'"
echo "  2. 'Please take the leaflet'"
echo "  3. 'Can you read the leaflet for me?'"
echo "  4. 'Let's go to the north'"
echo ""
echo "Starting game..."
echo ""
sleep 2

./zork-native game/zork1.z3 < /tmp/zork-demo-input.txt 2>&1 | \
  grep -v "^=" | grep -v "^Loading" | grep -v "^Using" | \
  tail -60 | head -50

rm -f /tmp/zork-demo-input.txt

echo ""
echo "=================================================="
echo "Demo complete! The LLM translated natural language"
echo "into classic Zork commands in real-time."
echo "=================================================="
