#!/bin/bash
# Play Zork on Tenstorrent Blackhole Hardware
# Simple launcher for running Z-machine interpreter on RISC-V cores

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors
BLUE='\033[0;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  🎮 Zork on Tenstorrent Blackhole RISC-V            ║${NC}"
echo -e "${BLUE}║  Running 1977 gaming on 2026 AI accelerator!        ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════╝${NC}"
echo

# Check if TT_METAL_RUNTIME_ROOT is set
if [ -z "$TT_METAL_RUNTIME_ROOT" ]; then
    echo -e "${YELLOW}⚠️  TT_METAL_RUNTIME_ROOT not set${NC}"
    echo -e "    Setting to default: /home/ttuser/tt-metal"
    export TT_METAL_RUNTIME_ROOT=/home/ttuser/tt-metal
fi

# Check if binary exists
if [ ! -f "build-host/test_zork_optimized" ]; then
    echo -e "${RED}❌ Error: build-host/test_zork_optimized not found${NC}"
    echo -e "    Please build first:"
    echo -e "    ${BLUE}cd build-host && cmake --build . --target test_zork_optimized${NC}"
    exit 1
fi

# Get batch count from argument (default: 5)
BATCHES=${1:-5}

if [ "$BATCHES" -gt 5 ]; then
    echo -e "${YELLOW}⚠️  Warning: More than 5 batches may cause device hangs${NC}"
    echo -e "    Recommended: 5 batches (proven stable)"
    read -p "    Continue anyway? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${BLUE}Using 5 batches (safe default)${NC}"
        BATCHES=5
    fi
fi

echo -e "${GREEN}Configuration:${NC}"
echo -e "  Batches: ${BATCHES}"
echo -e "  Instructions per batch: 100"
echo -e "  Total instructions: $((BATCHES * 100))"
echo -e "  Expected time: ~$((BATCHES + 3)) seconds"
echo

echo -e "${BLUE}[1/3]${NC} Checking device health..."
if ! tt-smi -s > /dev/null 2>&1; then
    echo -e "${RED}❌ Devices not accessible${NC}"
    echo -e "    Try: ${BLUE}sudo tt-cold-reboot${NC}"
    exit 1
fi
echo -e "      ✅ Devices healthy"

echo -e "${BLUE}[2/3]${NC} Launching Zork on RISC-V cores..."
echo

# Run with timeout to catch hangs
if timeout 120 ./build-host/test_zork_optimized "$BATCHES"; then
    echo
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║  ✅ Execution Complete!                               ║${NC}"
    echo -e "${GREEN}║  Zork ran successfully on Blackhole RISC-V           ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════╝${NC}"
    echo
    echo -e "${BLUE}[INFO]${NC} Next Steps:"
    echo -e "  - Run again: ${GREEN}./play-zork-hardware.sh${NC}"
    echo -e "  - More batches: ${GREEN}./play-zork-hardware.sh 10${NC} (may hang)"
    echo -e "  - If hung: ${YELLOW}sudo tt-cold-reboot${NC} or ${YELLOW}tt-smi -r 0${NC}"
    exit 0
else
    EXIT_CODE=$?
    echo
    echo -e "${RED}╔═══════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║  ❌ Execution Failed or Timed Out                     ║${NC}"
    echo -e "${RED}╚═══════════════════════════════════════════════════════╝${NC}"
    echo

    if [ $EXIT_CODE -eq 124 ]; then
        echo -e "${YELLOW}[ERROR]${NC} Program timed out after 2 minutes"
        echo -e "        This usually means the device hung"
    else
        echo -e "${YELLOW}[ERROR]${NC} Program exited with code $EXIT_CODE"
    fi

    echo
    echo -e "${BLUE}[RECOVERY]${NC} To recover from device hang:"
    echo -e "  1. Quick recovery: ${GREEN}sudo tt-cold-reboot${NC}"
    echo -e "  2. Full reset:     ${GREEN}tt-smi -r 0${NC}"
    echo -e "  3. Last resort:    ${GREEN}sudo reboot${NC}"
    echo

    exit $EXIT_CODE
fi
