#!/usr/bin/env bash
# demo-stage3.sh — Stage 3: Z-machine interpreter on QB2 RISC-V cores
# This demo shows what the hardware actually produces — and where it got stuck.
# Record with: asciinema rec -t "Zork on Tenstorrent — Stage 3" demos/stage3.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

echo ""
echo "╔══════════════════════════════════════════════════════════════"
echo "║  STAGE 3 — Z-machine runs on QB2 RISC-V cores  (TT-Lang)"
echo "╚══════════════════════════════════════════════════════════════"
echo ""
sleep 2

echo "The interpreter kernel (kernels/zork_interpreter_l1.cpp) runs"
echo "directly on the RISC-V cores embedded in the Blackhole chip."
echo "The game file lives in DRAM. The cores do the Z-machine work."
echo ""
echo "Firmware limits each kernel invocation to 10 instructions."
echo "We open a fresh device session per batch to stay within limits."
echo ""
sleep 3

echo "Running 20 batches × 10 instructions = 200 Z-machine instructions..."
echo "(Each batch: open device → load state → execute → save state → close device)"
echo ""
sleep 2

ZORK_BATCHES=20 python ttlang/zork_risc.py game/zork1.z3

echo ""
sleep 2
echo "─────────────────────────────────────────────────────────────────"
echo "That's the hardware thinking. Real RISC-V cores. Real game text."
echo ""
echo "The wall: the Zork opening needs ~400 instructions total."
echo "At 10 per batch, that's 40+ batches. The per-batch device"
echo "open/close overhead (~0.5 s each) makes it slow but workable."
echo ""
echo "What can't be fixed in Python: the firmware watchdog itself."
echo "interpret(20+) hangs when PRINT fires — a firmware-level limit"
echo "on QB2 that only the TT-Metal team can address."
echo ""
echo "This is proof-of-concept, not a playable game. Stage 4 is."
echo "─────────────────────────────────────────────────────────────────"
sleep 4
