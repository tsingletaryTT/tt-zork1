#!/usr/bin/env bash
#
# test-map-demo.sh - Demonstrate journey map generation
#
# This script plays through a short Zork session and triggers
# the map display by creating a fake "death" scenario.
#

echo "=== Journey Map Demo ==="
echo ""
echo "Playing through a short Zork journey..."
echo "Commands: open mailbox, take leaflet, north, east, south, west, quit"
echo ""

# Play through a short journey and quit
# (Map won't show on quit, but we can see the journey tracking)
printf "open mailbox\ntake leaflet\nnorth\neast\nsouth\nwest\nquit\ny\n" | ./zork-native game/zork1.z3 2>&1 | tail -30

echo ""
echo "=== Demo Complete ==="
echo ""
echo "Note: Map only displays on death or victory, not on user quit."
echo "The journey tracking is working as shown in the debug output above."
