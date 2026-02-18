#!/usr/bin/env bash
#
# demo-tui.sh - Demonstrate the split-screen TUI
#
# This script shows off the new TUI features with a clean display.
#

set -e

echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║                   ZORK - SPLIT-SCREEN TUI DEMO                  ║"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""
echo "Features:"
echo "  ✓ Header bar with game mode and LLM status"
echo "  ✓ Main game area (70% width) - scrolling Z-machine output"
echo "  ✓ Right sidebar (30% width):"
echo "    - ASCII art display (top)"
echo "    - Inventory list (middle)"
echo "    - Journey map (bottom)"
echo "  ✓ Footer for command input"
echo ""
echo "Controls:"
echo "  - Type commands normally"
echo "  - Use /help for slash commands"
echo "  - CTRL+C to quit"
echo ""
echo "To disable TUI and use plain text:"
echo "  export ZORK_TUI_DISABLE=1"
echo ""
echo "Press ENTER to start..."
read

# Run Zork with TUI enabled
./zork-native game/zork1.z3
