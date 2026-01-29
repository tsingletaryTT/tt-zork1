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

# Change to script directory so paths work correctly
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

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

# ═══════════════════════════════════════════════════════════
# Environment Setup
# ═══════════════════════════════════════════════════════════

# Check and set TT_METAL_RUNTIME_ROOT
if [ -z "$TT_METAL_RUNTIME_ROOT" ]; then
    echo -e "${YELLOW}⚠️  TT_METAL_RUNTIME_ROOT not set${NC}"

    # Try to auto-detect
    if [ -d "/home/ttuser/tt-metal" ]; then
        export TT_METAL_RUNTIME_ROOT=/home/ttuser/tt-metal
        echo -e "    Auto-detected: $TT_METAL_RUNTIME_ROOT"
    elif [ -d "$HOME/tt-metal" ]; then
        export TT_METAL_RUNTIME_ROOT=$HOME/tt-metal
        echo -e "    Auto-detected: $TT_METAL_RUNTIME_ROOT"
    else
        echo -e "${RED}❌ Error: Cannot find tt-metal installation${NC}"
        echo -e "    Please set TT_METAL_RUNTIME_ROOT manually:"
        echo -e "    ${BLUE}export TT_METAL_RUNTIME_ROOT=/path/to/tt-metal${NC}"
        exit 1
    fi
else
    echo -e "${GREEN}✓${NC} TT_METAL_RUNTIME_ROOT: $TT_METAL_RUNTIME_ROOT"
fi
echo ""

# Check if binary exists
if [ ! -f "$ZORK_BIN" ]; then
    echo -e "${RED}❌ Error: $ZORK_BIN not found!${NC}"
    echo "Please build first:"
    echo -e "  ${BLUE}cd build-host && cmake --build . --parallel && cd ..${NC}"
    exit 1
fi
echo -e "${GREEN}✓${NC} Binary found: $ZORK_BIN"

# Check if tt-smi is available (optional - for device health check)
if command -v tt-smi &> /dev/null; then
    echo -e "${GREEN}✓${NC} tt-smi available"

    # Quick device health check
    if tt-smi -s > /dev/null 2>&1; then
        echo -e "${GREEN}✓${NC} Device healthy"
    else
        echo -e "${YELLOW}⚠️  Device health check failed${NC}"
        echo -e "    You may need to reset:"
        echo -e "    ${BLUE}sudo tt-cold-reboot${NC}"
        read -p "    Continue anyway? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
else
    echo -e "${YELLOW}⚠️  tt-smi not found (skipping device check)${NC}"
fi
echo ""

# ═══════════════════════════════════════════════════════════
# Initialize Game
# ═══════════════════════════════════════════════════════════

echo -e "${BLUE}[System] Initializing game...${NC}"
echo "" > "$INPUT_FILE"  # Empty input for opening
rm -f "$STATE_FILE"      # Start fresh

echo -e "${BLUE}[System] Running opening sequence (5 batches = 500 instructions)...${NC}"
echo -e "${BLUE}          This will take ~5-8 seconds...${NC}"

# Run 5 batches to get opening text
if ! TT_METAL_RUNTIME_ROOT="$TT_METAL_RUNTIME_ROOT" "$ZORK_BIN" game/zork1.z3 5; then
    echo ""
    echo -e "${RED}❌ Failed to run opening sequence!${NC}"
    echo -e "    Possible causes:"
    echo -e "    1. Device not responding - try: ${BLUE}sudo tt-cold-reboot${NC}"
    echo -e "    2. Kernel timeout - this is a known issue with >5 batches"
    echo -e "    3. Missing dependencies - check TT-Metal installation"
    exit 1
fi

echo ""
echo -e "${GREEN}═══════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}✓ Game initialized! Ready for interactive commands.${NC}"
echo -e "${GREEN}═══════════════════════════════════════════════════════${NC}"
echo ""

echo -e "${YELLOW}Helpful Tips:${NC}"
echo -e "  • Each command takes ~3-5 seconds to process"
echo -e "  • Try: ${BLUE}look${NC}, ${BLUE}inventory${NC}, ${BLUE}north${NC}, ${BLUE}open mailbox${NC}"
echo -e "  • Type ${BLUE}help${NC} for command suggestions"
echo -e "  • Type ${BLUE}quit${NC} to exit"
echo -e "  • If device hangs, press Ctrl+C then run: ${BLUE}sudo tt-cold-reboot${NC}"
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

    echo -e "${BLUE}[System] Processing \"$cmd\"...${NC}"

    # Run 3 batches (300 instructions - enough for most commands)
    if ! TT_METAL_RUNTIME_ROOT="$TT_METAL_RUNTIME_ROOT" "$ZORK_BIN" game/zork1.z3 3 2>&1; then
        echo ""
        echo -e "${RED}❌ Kernel execution failed!${NC}"
        echo -e "    This might happen after several turns due to hardware limits."
        echo -e "    Options:"
        echo -e "    1. Reset device: ${BLUE}sudo tt-cold-reboot${NC}"
        echo -e "    2. Restart script (progress saved in state file)"
        echo -e "    3. Type 'quit' to exit gracefully"
        echo ""
        read -p "    Continue trying? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            break
        fi
        continue  # Skip to next turn
    fi

    echo ""
    ((turn++))
done

echo ""
echo -e "${GREEN}═══════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}Session Summary${NC}"
echo -e "${GREEN}═══════════════════════════════════════════════════════${NC}"
echo -e "  Turns completed: $((turn-1))"
echo -e "  Commands processed: $((turn-1))"
echo -e "  Total instructions: ~$((turn * 300))"
echo ""
echo -e "${BLUE}Thanks for playing Zork on AI hardware! 🎮${NC}"
echo ""
echo -e "To play again, just run: ${BLUE}./play-zork-interactive.sh${NC}"
