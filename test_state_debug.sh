#!/bin/bash
# test_state_debug.sh - Run instrumented kernel with full debug output
#
# This script enables all TT-Metal debug features to diagnose the state persistence hang:
# - WAYPOINT markers: Track execution progress
# - DPRINT logging: Verbose operation logging
# - Ring buffer: Post-mortem execution trace
# - Watcher: Polls device state and dumps on hang
#
# Expected: Batch 1 works, batch 2 hangs. Debug output will show WHERE it hangs.

set -e  # Exit on error

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  STATE PERSISTENCE DEBUG TEST - INSTRUMENTED KERNEL           ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Enable ALL debug features
export TT_METAL_WATCHER=5                    # Poll every 5 seconds
export TT_METAL_WATCHER_DUMP_ALL=1           # Dump all state on hang
export TT_METAL_DPRINT_CORES="(0,0)"         # Enable DPRINT on core (0,0)
export TT_METAL_DPRINT_CORES_DISABLE=""      # Don't disable any cores
export TT_METAL_DPRINT_FILE=/tmp/zork_dprint.log  # Save DPRINT output to file
export TT_METAL_RUNTIME_ROOT=/home/ttuser/tt-metal

echo "[DEBUG] Watcher enabled: poll every 5 seconds"
echo "[DEBUG] DPRINT enabled: core (0,0) -> /tmp/zork_dprint.log"
echo "[DEBUG] Ring buffer enabled: post-mortem trace available"
echo ""

# Clear any old watcher logs
rm -f generated/watcher/watcher.log 2>/dev/null || true
rm -f /tmp/zork_dprint.log 2>/dev/null || true

echo "[DEBUG] Running 4-batch test (expect hang at batch 2)..."
echo "[DEBUG] If it hangs, let watcher poll (wait 10 seconds), then Ctrl+C"
echo ""

# Run the test - expect batch 1 success, batch 2 hang
./build-host/zork_on_blackhole game/zork1.z3 4 2>&1 | tee /tmp/zork_test_output.log

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  ANALYZING DEBUG OUTPUT...                                    ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Show watcher log if it exists
if [ -f generated/watcher/watcher.log ]; then
    echo "━━━━ WATCHER LOG (last 50 lines) ━━━━"
    tail -50 generated/watcher/watcher.log
    echo ""
else
    echo "⚠️  No watcher log found at generated/watcher/watcher.log"
    echo ""
fi

# Show DPRINT log if it exists
if [ -f /tmp/zork_dprint.log ]; then
    echo "━━━━ DPRINT LOG ━━━━"
    cat /tmp/zork_dprint.log
    echo ""
else
    echo "⚠️  No DPRINT log found at /tmp/zork_dprint.log"
    echo ""
fi

# Show last waypoint hit
echo "━━━━ LAST WAYPOINT (from watcher log) ━━━━"
if [ -f generated/watcher/watcher.log ]; then
    grep -i "waypoint" generated/watcher/watcher.log | tail -5 || echo "No waypoints found"
else
    echo "No watcher log available"
fi
echo ""

# Show ring buffer data
echo "━━━━ RING BUFFER (from watcher log) ━━━━"
if [ -f generated/watcher/watcher.log ]; then
    grep -i "ring_buf" generated/watcher/watcher.log | tail -10 || echo "No ring buffer data found"
else
    echo "No watcher log available"
fi
echo ""

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  ANALYSIS COMPLETE - Check output above for hang location     ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "Key markers to look for:"
echo "  SS1, SS2, SNA = State initialization"
echo "  SRD = State Read (NoC async read)"
echo "  SRB = State Read Barrier (waiting for read)"
echo "  SRL, SRI = State data access after read"
echo "  SRS/SRF = Resume or Fresh init"
echo "  SSV = State Save (updating counters)"
echo "  SWR = State Write (NoC async write)"
echo "  SWB = State Write Barrier (waiting for write)"
echo "  SDN = State Done (all complete)"
echo ""
echo "Ring buffer markers:"
echo "  0xDEAD0001 = State section start"
echo "  0xBEEF0001 = Before NoC read"
echo "  0xBEEF0002 = Before read barrier"
echo "  0xBEEF0003 = Read complete"
echo "  0xCAFE0001 = Before NoC write"
echo "  0xCAFE0002 = Before write barrier"
echo "  0xDEAD0002 = State section complete"
echo ""
