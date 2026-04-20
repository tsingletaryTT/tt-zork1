#!/usr/bin/env bash
# Run Zork in the TT-Lang Python simulator (no hardware required).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/.."
source ~/code/tt-lang/build/env/activate
export PYTHONPATH=~/code/tt-lang/python
exec python ttlang/zork_ttlang.py "${1:-game/zork1.z3}"
