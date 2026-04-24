#!/usr/bin/env bash
# demo-stage1.sh — Stage 1: Pure Python Z-machine
# Record with: asciinema rec -t "Zork on Tenstorrent — Stage 1" demos/stage1.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

echo ""
echo "╔══════════════════════════════════════════════════════════════"
echo "║  STAGE 1 — Pure Python Z-machine  (no hardware required)"
echo "╚══════════════════════════════════════════════════════════════"
echo ""
sleep 2

# Pipe scripted commands with human-paced delays
{
  sleep 4      # let opening text render
  echo "look"
  sleep 3
  echo "open mailbox"
  sleep 3
  echo "take leaflet"
  sleep 3
  echo "read leaflet"
  sleep 5
  echo "quit"
} | python play.py --stage sim --game game/zork1.z3
