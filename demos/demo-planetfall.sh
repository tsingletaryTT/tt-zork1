#!/usr/bin/env bash
# demo-planetfall.sh — Planetfall on Blackhole DRAM
#
# ZIL source: https://github.com/historicalsource/planetfall
# Compiled story file: planetfall.zip (Z-machine V3, Release 39)
# Not redistributed — downloaded fresh each run from historicalsource.
#
# Shows: the ship-deck opener with Ensign Seventh Class and their
# scrub brush, the alien ambassador who ambles past leaving green slime,
# and movement through the Feinstein's corridor to the Reactor Lobby.
#
# Record with:
#   COLUMNS=120 LINES=35 \
#     asciinema rec -t "Planetfall — Blackhole DRAM" demos/planetfall.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

echo ""
echo "╔══════════════════════════════════════════════════════════════"
echo "║  PLANETFALL"
echo "║  Infocom, 1983 · Steve Meretzky · ZIL source: historicalsource"
echo "║"
echo "║  Running on Tenstorrent Blackhole DRAM — stage 2"
echo "║  Game binary lives on silicon; Python interpreter on host."
echo "╚══════════════════════════════════════════════════════════════"
echo ""
sleep 3

{
  sleep 6        # long opening narration about the scrub brush
  echo "inventory"
  sleep 4        # "A Patrol uniform... a scrub brush"
  echo "look"
  sleep 3
  echo "port"
  sleep 3        # pod bulkhead is closed
  echo "open bulkhead"
  sleep 4        # "Why open the door... if there's no emergency?" + ambassador celery
  echo "starboard"
  sleep 3
  echo "look"
  sleep 4        # Reactor Lobby
  echo "quit"
  sleep 2
  echo "y"
} | python play.py --stage device --game game/planetfall.z3
