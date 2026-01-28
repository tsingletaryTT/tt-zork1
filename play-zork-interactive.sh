#!/bin/bash
# play-zork-interactive.sh - Interactive Zork gameplay on Blackhole RISC-V
#
# This script enables TRUE INTERACTIVE GAMEPLAY by:
# 1. Running opening sequence (no input needed)
# 2. Prompting user for commands
# 3. Uploading command to input buffer
# 4. Executing batch with READ opcode
# 5. Displaying game response
# 6. Looping until user quits
#
# Usage: ./play-zork-interactive.sh

set -e

ZORK_BIN="./build-host/zork_on_blackhole"
INPUT_FILE="/tmp/zork_input.txt"
STATE_FILE="/tmp/zork_state.bin"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo "╔══════════════════════════════════════════════════════════╗"
echo "║  ZORK I - Interactive Mode on Blackhole RISC-V"
echo "║  Type 'quit' to exit, 'help' for commands"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""

# Check if binary exists
if [ ! -f "$ZORK_BIN" ]; then
    echo -e "${RED}Error: $ZORK_BIN not found!${NC}"
    echo "Please build first:"
    echo "  cd build-host && cmake --build . --parallel"
    exit 1
fi

# Initialize - run opening sequence (5 batches = 500 instructions)
echo -e "${BLUE}[System] Running opening sequence...${NC}"
echo "" > "$INPUT_FILE"  # Empty input for opening
rm -f "$STATE_FILE"      # Start fresh

# Run 5 batches to get opening text
if ! "$ZORK_BIN" game/zork1.z3 5; then
    echo -e "${RED}Failed to run opening sequence!${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}═══════════════════════════════════════${NC}"
echo -e "${GREEN}Game initialized! Ready for commands.${NC}"
echo -e "${GREEN}═══════════════════════════════════════${NC}"
echo ""

# Interactive loop
turn=1
while true; do
    # Prompt for command
    echo -e "${YELLOW}Turn $turn >${NC} "
    read -r cmd

    # Handle special commands
    if [ "$cmd" == "quit" ] || [ "$cmd" == "exit" ]; then
        echo "Thanks for playing!"
        break
    fi

    if [ "$cmd" == "help" ]; then
        echo "Common commands:"
        echo "  look, inventory, north/south/east/west"
        echo "  take <item>, drop <item>, open <item>"
        echo "  examine <item>, read <item>"
        echo "  quit - exit game"
        continue
    fi

    if [ -z "$cmd" ]; then
        echo "Please enter a command (or 'help' for suggestions)"
        continue
    fi

    # Write command to input file
    echo "$cmd" > "$INPUT_FILE"

    # Update input buffer before running kernel
    # For now, we'll rely on the host program to read from file
    # In the future, we could use EnqueueWriteMeshBuffer to update input buffer

    echo -e "${BLUE}[System] Processing command...${NC}"

    # Run 2-5 batches (200-500 instructions should be enough for one command)
    # Start with 3 batches as a reasonable middle ground
    if ! "$ZORK_BIN" game/zork1.z3 3; then
        echo -e "${RED}Kernel execution failed!${NC}"
        echo "You may need to reset the device:"
        echo "  sudo tt-cold-reboot"
        break
    fi

    echo ""
    ((turn++))
done

echo ""
echo "Session ended after $((turn-1)) turns."
