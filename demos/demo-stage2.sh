#!/usr/bin/env bash
# demo-stage2.sh — Stage 2: Game data on QB2 DRAM
# Record with: asciinema rec -t "Zork on Tenstorrent — Stage 2" demos/stage2.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  STAGE 2 — Game data lives in QB2 DRAM  (Tenstorrent chip) ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
sleep 2

{
  sleep 12     # QB2 device init + game upload
  echo "look"
  sleep 3
  echo "open mailbox"
  sleep 3
  echo "take leaflet"
  sleep 3
  echo "quit"
} | python play.py --stage device --game game/zork1.z3
