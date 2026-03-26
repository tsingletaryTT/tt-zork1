#!/bin/bash
# Visual test of TUI - shows actual output without escape codes

echo "╔════════════════════════════════════════════════════════╗"
echo "║  Testing TUI Display (Staircasing Fix Verification)  ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

echo "Starting Zork with TUI..."
echo ""

# Run with a very short timeout and capture initialization
timeout 3 ./zork-native game/zork1.z3 <<EOF 2>&1 | head -100 || true
quit
y
EOF

echo ""
echo "Test complete!"
echo ""
echo "What to look for:"
echo "  ✅ Messages should appear in game window (left side)"
echo "  ✅ Sidebar should be clean (right side)"
echo "  ✅ No diagonal/staircasing text"
echo "  ✅ No overlapping sections"
