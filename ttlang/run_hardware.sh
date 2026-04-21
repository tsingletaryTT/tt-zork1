#!/usr/bin/env bash
# Run Zork with game data in QB2 DRAM (Python interpreter on host).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/.."
source ~/code/tt-lang/build/env/activate
# activate already sets PYTHONPATH correctly — do NOT add extra PYTHONPATH
exec python ttlang/zork_device.py "${1:-game/zork1.z3}"
