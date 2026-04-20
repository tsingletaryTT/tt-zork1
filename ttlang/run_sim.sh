#!/usr/bin/env bash
# Run Zork in the TT-Lang Python simulator (no hardware required).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/.."
source ~/code/tt-lang/build/env/activate
# activate already sets PYTHONPATH correctly (includes ttnn, tt-metal, build/python_packages)
exec python ttlang/zork_ttlang.py "${1:-game/zork1.z3}"
