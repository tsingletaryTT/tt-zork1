#!/bin/bash
# Run Zork interpreter in batches using consecutive single-shot executions
# This works around the device init timeout issue in zork_batched.cpp

set -e

GAME_FILE="${1:-game/zork1.z3}"
NUM_BATCHES="${2:-10}"
OUTPUT_DIR="/tmp/zork-batches"

echo "╔══════════════════════════════════════════════════════════╗"
echo "║  ZORK I - Batched Execution (Shell Script Approach)    ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""
echo "Game file: $GAME_FILE"
echo "Batches: $NUM_BATCHES (each batch = 100 instructions)"
echo "Output dir: $OUTPUT_DIR"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Run batches
echo "[1/3] Running batched execution..."
for i in $(seq 1 $NUM_BATCHES); do
    echo -n "  Batch $i/$NUM_BATCHES..."

    # Run single-shot interpreter
    TT_METAL_RUNTIME_ROOT=/home/ttuser/tt-metal \
        timeout 30 ./build-host/zork_on_blackhole "$GAME_FILE" \
        > "$OUTPUT_DIR/batch_$i.log" 2>&1

    # Check if succeeded
    if [ $? -eq 0 ]; then
        echo " done"
    else
        echo " FAILED"
        echo "Check $OUTPUT_DIR/batch_$i.log for details"
        exit 1
    fi

    # Small delay between batches
    sleep 1
done

echo ""
echo "[2/3] Extracting Zork output from batches..."

# Extract actual game output (between the box borders)
for i in $(seq 1 $NUM_BATCHES); do
    # Look for text between "EXECUTING Z-MACHINE CODE" and "EXECUTION COMPLETE"
    sed -n '/=== EXECUTING Z-MACHINE CODE ===/,/=== EXECUTION COMPLETE ===/p' \
        "$OUTPUT_DIR/batch_$i.log" | \
        grep -v "===" | \
        grep -v "^\s*$" \
        > "$OUTPUT_DIR/output_$i.txt" 2>/dev/null || true
done

echo ""
echo "[3/3] Accumulated output from all batches:"
echo "══════════════════════════════════════════════════════════"

# Concatenate all outputs
cat "$OUTPUT_DIR"/output_*.txt 2>/dev/null || echo "(No output captured)"

echo "══════════════════════════════════════════════════════════"
echo ""
echo "✓ Batched execution complete!"
echo "  Individual logs: $OUTPUT_DIR/batch_*.log"
echo "  Extracted output: $OUTPUT_DIR/output_*.txt"
echo ""

# Show statistics
SUCCESS_COUNT=$(ls "$OUTPUT_DIR"/batch_*.log 2>/dev/null | wc -l)
echo "Batches executed: $SUCCESS_COUNT/$NUM_BATCHES"
echo ""
