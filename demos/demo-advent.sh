#!/usr/bin/env bash
# demo-advent.sh — Colossal Cave Adventure (Z5) on the Python Z-machine
#
# Colossal Cave Adventure is the 1976 original text adventure by Will Crowther
# and Don Woods — the direct ancestor of Zork. This demo shows the Graham Nelson
# Z5 port running on the same Python Z-machine interpreter that handles Zork I–III,
# demonstrating V5 compatibility: CALL_VS2 double type-bytes, AREAD store byte,
# and a 9-byte dictionary key layout (vs Z3's 7-byte entries).
#
# Record with:
#   COLUMNS=120 LINES=35 \
#     asciinema rec -t "Colossal Cave Adventure — Z5 on TT-Zork" demos/advent.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

echo ""
echo "╔══════════════════════════════════════════════════════════════"
echo "║  Colossal Cave Adventure — Z-machine V5"
echo "║  Will Crowther (1976) · Don Woods (1977)"
echo "║  Z5 port by Graham Nelson (1994)"
echo "║"
echo "║  The original text adventure — the ancestor of Zork."
echo "║  Running on the same Python Z-machine as Zork I–III."
echo "╚══════════════════════════════════════════════════════════════"
echo ""
sleep 3

# Scripted session with human-paced delays.
# Classic opening moves: start at End Of Road, enter the well house,
# take the brass lamp, step back outside, and wander into the forest.
{
  sleep 5        # opening text
  echo "look"
  sleep 4
  echo "in"
  sleep 4
  echo "get lamp"
  sleep 3
  echo "out"
  sleep 3
  echo "north"
  sleep 3
  echo "look"
  sleep 4
  echo "south"
  sleep 3
  echo "look"
  sleep 4
  echo "quit"
  sleep 2
  echo "y"
} | python play.py --stage sim --game game/advent.z5
