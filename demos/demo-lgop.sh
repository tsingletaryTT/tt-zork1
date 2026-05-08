#!/usr/bin/env bash
# demo-lgop.sh — Leather Goddesses of Phobos on Blackhole DRAM
#
# ZIL source: https://github.com/historicalsource/leathergoddesses
# Compiled story file: x1.zip (Z-machine V3, Release 59)
# Not redistributed — downloaded fresh each run from historicalsource.
#
# Shows: the 1936 Upper Sandusky, Ohio bar setting, the satirical
# opening disclaimer, and the fateful first puzzle (choosing a bathroom).
#
# Record with:
#   COLUMNS=120 LINES=35 \
#     asciinema rec -t "LGOP — Blackhole DRAM" demos/lgop.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

echo ""
echo "╔══════════════════════════════════════════════════════════════"
echo "║  LEATHER GODDESSES OF PHOBOS"
echo "║  Infocom, 1986 · Steve Meretzky · ZIL source: historicalsource"
echo "║"
echo "║  Running on Tenstorrent Blackhole DRAM — stage 2"
echo "║  Game binary lives on silicon; Python interpreter on host."
echo "╚══════════════════════════════════════════════════════════════"
echo ""
sleep 3

{
  sleep 5        # disclaimer + press enter
  echo ""        # press enter to start
  sleep 5        # "Upper Sandusky, Ohio. 1936."
  echo "look"
  sleep 4
  echo "northwest"
  sleep 4        # Gents' Room
  echo "look"
  sleep 4
  echo "southeast"
  sleep 3        # back to bar
  echo "look"
  sleep 3
  echo "quit"
  sleep 2
  echo "y"
} | python play.py --stage device --game game/lgop.z3
