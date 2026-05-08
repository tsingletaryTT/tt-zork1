#!/usr/bin/env bash
# fetch-infocom.sh — Download Infocom game files from historicalsource on GitHub.
#
# These files are NOT included in this repository to avoid redistribution.
# Run this script once to download them into game/ before running the demos.
#
# Sources:
#   https://github.com/historicalsource/hitchhikersguide  (s4.zip → hhgg.z3)
#   https://github.com/historicalsource/planetfall         (planetfall.zip → planetfall.z3)
#   https://github.com/historicalsource/leathergoddesses   (x1.zip → lgop.z3)
#
# The .zip extension is Infocom's original "ZIP" (Z-machine Interpretive Package)
# format — not a compressed archive. These are the original compiled Z-machine V3
# story files from Infocom's in-house build system.

set -euo pipefail
cd "$(dirname "$0")"

BASE="https://raw.githubusercontent.com/historicalsource"

download() {
    local url="$1"
    local dest="$2"
    if [[ -f "$dest" ]]; then
        echo "  already exists: $dest (skipping)"
        return
    fi
    echo "  downloading: $dest"
    curl -sL "$url" -o "$dest"
    local version
    version=$(python3 -c "import sys; b=open('$dest','rb').read(1); print(b[0])")
    echo "  verified: Z-machine version $version"
}

echo ""
echo "╔══════════════════════════════════════════════════════════════"
echo "║  Fetching Infocom game files from historicalsource on GitHub"
echo "╚══════════════════════════════════════════════════════════════"
echo ""

download "$BASE/hitchhikersguide/master/s4.zip"       "hhgg.z3"
download "$BASE/planetfall/master/planetfall.zip"     "planetfall.z3"
download "$BASE/leathergoddesses/master/x1.zip"       "lgop.z3"

echo ""
echo "Done. Files in game/:"
ls -lh hhgg.z3 planetfall.z3 lgop.z3 2>/dev/null
echo ""
echo "Run: python play.py --game game/hhgg.z3"
echo "     python play.py --game game/planetfall.z3"
echo "     python play.py --game game/lgop.z3"
