#!/bin/bash
# Play Zork with LLM natural language translation!

export ZORK_LLM_ENABLED=1

echo "╔════════════════════════════════════════════════════════════╗"
echo "║  ZORK I with Natural Language Translation (via LLM)       ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
echo "🤖 Translator Agent: ENABLED (Chip 0 - Qwen3-0.6B)"
echo "📍 Endpoint: http://localhost:8000"
echo ""
echo "Try natural language like:"
echo "  'I want to open the mailbox'"
echo "  'Please pick up the leaflet'"
echo "  'Can you go north?'"
echo ""
echo "Or use exact commands:"
echo "  'open mailbox', 'take leaflet', 'north'"
echo ""
echo "Type 'quit' to exit"
echo "════════════════════════════════════════════════════════════"
echo ""

./zork-native game/zork1.z3
