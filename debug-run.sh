#!/bin/bash
# debug-run.sh - Run Zork with comprehensive debugging and monitoring
#
# This script integrates TT-Metal debugging tools:
# - watcher_dump: Captures device state before/after execution
# - Tracy profiler: Performance profiling
# - Logs all output for analysis

set -e

# Configuration
BATCHES=${1:-4}
LOG_DIR="/tmp/zork-debug-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$LOG_DIR"

echo "╔═══════════════════════════════════════════════╗"
echo "║  Zork Debug Run with TT-Metal Tools          ║"
echo "╠═══════════════════════════════════════════════╣"
echo "║  Batches: $BATCHES                                  ║"
echo "║  Log dir: $LOG_DIR"
echo "╚═══════════════════════════════════════════════╝"
echo

# Step 1: Capture device state BEFORE execution
echo "[1/5] Capturing device state (watcher_dump)..."
TT_METAL_RUNTIME_ROOT=/home/ttuser/tt-metal \
    /home/ttuser/tt-metal/build_Release/tools/watcher_dump \
    > "$LOG_DIR/watcher-before.log" 2>&1 || echo "  (watcher_dump unavailable or failed)"

# Step 2: Get device info
echo "[2/5] Capturing device info (tt-smi)..."
tt-smi -s > "$LOG_DIR/tt-smi-before.json" 2>&1

# Step 3: Run Zork with full logging
echo "[3/5] Running Zork ($BATCHES batches)..."
echo "  Start time: $(date +%H:%M:%S)"

TT_METAL_RUNTIME_ROOT=/home/ttuser/tt-metal \
    ./build-host/zork_on_blackhole game/zork1.z3 $BATCHES \
    > "$LOG_DIR/zork-output.log" 2>&1

EXIT_CODE=$?
echo "  End time: $(date +%H:%M:%S)"
echo "  Exit code: $EXIT_CODE"

# Step 4: Capture device state AFTER execution
echo "[4/5] Capturing post-run device state..."
tt-smi -s > "$LOG_DIR/tt-smi-after.json" 2>&1

TT_METAL_RUNTIME_ROOT=/home/ttuser/tt-metal \
    /home/ttuser/tt-metal/build_Release/tools/watcher_dump \
    > "$LOG_DIR/watcher-after.log" 2>&1 || echo "  (watcher_dump unavailable or failed)"

# Step 5: Analyze results
echo "[5/5] Analysis..."
echo

if [ $EXIT_CODE -eq 0 ]; then
    echo "✅ SUCCESS!"

    # Show batch completion
    echo
    echo "Batch Progress:"
    grep -E "(Batch|complete)" "$LOG_DIR/zork-output.log" || echo "  (no batch info found)"

    # Show output
    echo
    echo "Game Output:"
    grep -A20 "ACCUMULATED ZORK OUTPUT" "$LOG_DIR/zork-output.log" | head -30 || echo "  (no output found)"

else
    echo "❌ FAILED (exit code: $EXIT_CODE)"

    # Show errors
    echo
    echo "Errors:"
    grep -i "error\|timeout\|failed" "$LOG_DIR/zork-output.log" | tail -10 || echo "  (no errors found)"
fi

echo
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Full logs saved to: $LOG_DIR"
echo "  - zork-output.log      : Full execution log"
echo "  - tt-smi-before.json   : Device state before"
echo "  - tt-smi-after.json    : Device state after"
echo "  - watcher-before.log   : Watcher dump before"
echo "  - watcher-after.log    : Watcher dump after"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

exit $EXIT_CODE
