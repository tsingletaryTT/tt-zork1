#!/usr/bin/env bash
# demo-stage3.sh — Stage 3: Interpreter on RISC-V cores
# Record with: asciinema rec -t "Zork on Tenstorrent — Stage 3" demos/stage3.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  STAGE 3 — Z-machine runs on QB2 RISC-V cores  (TT-Lang)  ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
sleep 2

{
  sleep 60     # kernel compile + 5 batches × 40 instructions
  echo "open mailbox"
  sleep 20
  echo "quit"
} | python play.py --stage risc-v --game game/zork1.z3
