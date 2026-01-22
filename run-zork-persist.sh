#!/bin/bash
# Run Zork with state persistence across multiple invocations
# Resets device between runs to work around device locking issue

set -e

GAME_FILE="${1:-game/zork1.z3}"
NUM_RUNS="${2:-5}"

echo "╔══════════════════════════════════════════════════════════╗"
echo "║  ZORK I - Persistent State Across Runs                  ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""
echo "Game: $GAME_FILE"
echo "Runs: $NUM_RUNS (each run = 100 instructions)"
echo "State file: /tmp/zork_state.bin"
echo ""

# Optional: Start fresh
if [ "$3" == "fresh" ]; then
    echo "Starting fresh (deleting old state)..."
    rm -f /tmp/zork_state.bin
    echo ""
fi

# Run multiple times with reset between each
for i in $(seq 1 $NUM_RUNS); do
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Run $i/$NUM_RUNS"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    # Reset device before each run
    if [ $i -gt 1 ]; then
        echo "[Resetting device for run $i...]"
        tt-smi -r 0 1 > /dev/null 2>&1
        sleep 3
    fi

    # Run interpreter and capture full output
    OUTPUT=$(TT_METAL_RUNTIME_ROOT=/home/ttuser/tt-metal \
        timeout 30 ./build-host/zork_on_blackhole "$GAME_FILE" 1 2>&1 || true)

    # Show state info
    echo "$OUTPUT" | grep -E "No previous state|Loaded previous|Saved state" || true

    # Extract and show game output
    echo "$OUTPUT" | sed -n '/ACCUMULATED ZORK OUTPUT/,/╚════/p' | \
        grep -v "═" | grep -v "ACCUMULATED ZORK OUTPUT" | head -15

    echo ""
done

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✓ Completed $NUM_RUNS runs with persistent state!"
echo ""
echo "State file saved at: /tmp/zork_state.bin"
echo "To continue from where you left off, run again without 'fresh'"
echo "To start over, run with: $0 $GAME_FILE $NUM_RUNS fresh"
echo ""
