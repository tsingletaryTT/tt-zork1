#!/usr/bin/env bash
# demo-hhgg.sh — The Hitchhiker's Guide to the Galaxy on Blackhole DRAM
#
# ZIL source: https://github.com/historicalsource/hitchhikersguide
# Compiled story file: s4.zip (Z-machine V3, Release 60)
# Not redistributed — downloaded fresh each run from historicalsource.
#
# Shows: dark bedroom opener, the legendary "turn on light" response,
# the dressing gown with its mysterious pocket contents, and movement
# to the sitting room where Arthur's adventure truly begins.
#
# Record with:
#   COLUMNS=120 LINES=35 \
#     asciinema rec -t "HHGG — Blackhole DRAM" demos/hhgg.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

echo ""
echo "╔══════════════════════════════════════════════════════════════"
echo "║  THE HITCHHIKER'S GUIDE TO THE GALAXY"
echo "║  Infocom, 1984 · Douglas Adams · ZIL source: historicalsource"
echo "║"
echo "║  Running on Tenstorrent Blackhole DRAM — stage 2"
echo "║  Game binary lives on silicon; Python interpreter on host."
echo "╚══════════════════════════════════════════════════════════════"
echo ""
sleep 3

{
  sleep 5        # opening text — "You wake up..."
  echo "turn on light"
  sleep 5        # "Good start to the day. Pity it's going to be the worst one of your life."
  echo "get up"
  sleep 3
  echo "take gown"
  sleep 3
  echo "wear gown"
  sleep 3
  echo "open pocket"
  sleep 4        # "a buffered analgesic, pocket fluff, and a thing your aunt gave you..."
  echo "look in pocket"
  sleep 4
  echo "south"
  sleep 3
  echo "south"
  sleep 4
  echo "look"
  sleep 4
  echo "quit"
  sleep 2
  echo "y"
} | python play.py --stage device --game game/hhgg.z3
