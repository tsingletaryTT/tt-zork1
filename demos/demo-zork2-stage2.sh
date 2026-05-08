#!/usr/bin/env bash
# demo-zork2-stage2.sh — Stage 2: Zork II game data on QB2 DRAM
# Record with: asciinema rec -t "Zork II on Tenstorrent — Stage 2" demos/zork2-stage2.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

echo ""
echo "╔══════════════════════════════════════════════════════════════"
echo "║  STAGE 1 — Zork II: The Wizard of Frobozz"
echo "║  Pure Python Z-machine V3 interpreter, game 2 of 3."
echo "║  Same engine, different world: the Wizard of Frobozz awaits."
echo "╚══════════════════════════════════════════════════════════════"
echo ""
sleep 2

{
  sleep 3
  echo "look"
  sleep 3
  echo "take lantern"
  sleep 3
  echo "take sword"
  sleep 3
  echo "south"
  sleep 3
  echo "quit"
} | python play.py --stage sim --game game/zork2.z3
