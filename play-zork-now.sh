#!/bin/bash
# Quick Zork launcher

echo "🎮 ZORK I - The Great Underground Empire"
echo "========================================="
echo ""
echo "LLM Translation: Disabled (use exact commands)"
echo "Servers Status:"
./scripts/check-server-health.sh | grep "Summary:" 
echo ""
echo "Commands:"
echo "  look, north, south, east, west"
echo "  take [item], drop [item]"
echo "  open [thing], close [thing]"
echo "  read [item], examine [item]"
echo "  quit - exit game"
echo ""
echo "========================================="
echo ""

# Disable LLM for now (use exact commands)
export ZORK_LLM_ENABLED=0

./zork-native game/zork1.z3
