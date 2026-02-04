#!/bin/bash
# Zork REPL Mode - Fast Interactive Gameplay
# Keeps device warm and kernel compiled for minimal latency between commands
#
# Architecture: Device initialized ONCE → Kernel compiled ONCE → Reuse forever
# Expected latency: 100-500ms per command (vs 3-10s for batched mode)
#
# Usage: ./play-zork-repl.sh

set -e  # Exit on error

cd /home/ttuser/code/tt-zork1

echo "╔══════════════════════════════════════════════════════════╗"
echo "║  Zork I on Tenstorrent Blackhole - REPL Mode            ║"
echo "╠══════════════════════════════════════════════════════════╣"
echo "║  Fast interactive mode with device kept warm             ║"
echo "║  Expected latency: 100-500ms per command                 ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo

# Check prerequisites
echo "[Check] Verifying prerequisites..."

if [ ! -f "build-host/zork_repl" ]; then
    echo "❌ zork_repl not found. Building..."
    cd build-host && cmake --build . --target zork_repl --parallel && cd ..
    echo "✓ Build complete"
fi

if [ ! -f "game/zork1.z3" ]; then
    echo "❌ Game file not found: game/zork1.z3"
    exit 1
fi

# Check device health
echo "[Check] Checking device health..."
if ! tt-smi -s > /dev/null 2>&1; then
    echo "❌ Device not responding!"
    echo
    echo "Recovery options:"
    echo "  1. Try: sudo tt-cold-reboot"
    echo "  2. Try: tt-smi -r 0"
    echo "  3. Check device status: tt-smi"
    exit 1
fi

# Check chip status
CHIP_STATUS=$(tt-smi -s 2>/dev/null | jq -r '.devices[] | select(.device_id == 0) | .chip_status' || echo "unknown")
if [ "$CHIP_STATUS" != "ACTIVE" ]; then
    echo "⚠️  Warning: Chip 0 status is $CHIP_STATUS (expected ACTIVE)"
    echo "    REPL will use chip 1 if available"
fi

echo "✓ Device healthy"
echo

# Set TT-Metal runtime
export TT_METAL_RUNTIME_ROOT=/home/ttuser/tt-metal

# Launch REPL
echo "═══════════════════════════════════════════════════════════"
echo "Starting REPL mode..."
echo "═══════════════════════════════════════════════════════════"
echo

./build-host/zork_repl game/zork1.z3

# Cleanup message
echo
echo "═══════════════════════════════════════════════════════════"
echo "REPL session ended. Thanks for playing!"
echo "═══════════════════════════════════════════════════════════"
