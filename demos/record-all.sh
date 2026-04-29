#!/usr/bin/env bash
# record-all.sh — Record all demos + simultaneous hardware visualization
#
# For each demo this script records two cast files in parallel:
#   <name>.cast    — the demo itself (game play, output, narrative)
#   <name>-hw.cast — tt-toplike arcade mode running throughout
#
# The hw cast tells the hardware story:
#   stage1: chip mostly idle  (pure Python, no hardware used)
#   stage2: DRAM activity     (86 KB game file lives on-chip DRAM)
#   stage3: RISC-V cores lit  (Z-machine interpreter on Blackhole silicon)
#   hybrid: Tensix cores hot  (Llama-3.3-70B inference rewrites each response)
#   ai:     Tensix cores hot  (Llama-3.3-70B plays 30 turns end-to-end)
#
# Usage:
#   ./demos/record-all.sh                   # all five demos in order
#   ./demos/record-all.sh stage1            # just Stage 1
#   ./demos/record-all.sh stage2 stage3     # Stages 2 and 3
#   ./demos/record-all.sh hybrid ai         # the two LLM demos
#
# Requirements:
#   sudo apt install asciinema
#   source ~/code/tt-lang/build/env/activate   (done by each demo script)
#
# hybrid and ai prerequisites:
#   tt-inference-server must be running with Llama-3.3-70B-Instruct on :8000.
#   Quick check: curl -s http://localhost:8000/v1/models

set -euo pipefail
cd "$(dirname "$0")/.."

# ── Configuration ─────────────────────────────────────────────────────────────

HW_CMD="tt-toplike -m arcade --json -q"
HW_COLS=160
HW_ROWS=42
HW_HEAD=2
HW_TAIL=4

LLM_URL="${ZORK_LLM_URL:-http://localhost:8000/v1/chat/completions}"

# ── Demo registry ─────────────────────────────────────────────────────────────
# Each entry: "name|script|cols|rows|needs_llm"
# cols/rows are the terminal size for the GAME recording (not hw).
# ai uses 160x40 to give the TUI enough room.

DEMOS=(
    "stage1|demos/demo-stage1.sh|120|35|no"
    "stage2|demos/demo-stage2.sh|120|35|no"
    "stage3|demos/demo-stage3.sh|120|35|no"
    "hybrid|demos/demo-hybrid.sh|120|35|yes"
    "ai|demos/demo-ai.sh|160|40|yes"
)

# ── Stage list ────────────────────────────────────────────────────────────────

if [[ $# -eq 0 ]]; then
    TARGETS=(stage1 stage2 stage3 hybrid ai)
else
    TARGETS=("$@")
fi

# ── Helper: record one demo ───────────────────────────────────────────────────

record_demo() {
    local name="$1"
    local script cols rows needs_llm entry

    # Find matching entry in registry
    entry=""
    for d in "${DEMOS[@]}"; do
        if [[ "$d" == "${name}|"* ]]; then
            entry="$d"
            break
        fi
    done

    if [[ -z "$entry" ]]; then
        echo "Unknown demo: ${name}. Valid names: stage1 stage2 stage3 hybrid ai"
        return 1
    fi

    IFS='|' read -r _ script cols rows needs_llm <<< "$entry"

    local main_cast="demos/${name}.cast"
    local hw_cast="demos/${name}-hw.cast"
    local main_title="Zork on Tenstorrent — ${name}"
    local hw_title="Zork on Tenstorrent — ${name} hardware"

    echo ""
    echo "╔══════════════════════════════════════════════════════"
    echo "║  Demo: ${name}"
    echo "║  Game cast:  ${main_cast}"
    echo "║  HW cast:    ${hw_cast}"
    echo "╚══════════════════════════════════════════════════════"

    # Pre-flight: LLM server check for hybrid and ai demos.
    if [[ "$needs_llm" == "yes" ]]; then
        echo "  Checking LLM server at ${LLM_URL} ..."
        if ! curl -sf --max-time 5 "${LLM_URL%/chat/completions}/models" >/dev/null 2>&1; then
            echo ""
            echo "  ERROR: LLM server not reachable at ${LLM_URL}"
            echo "  Start tt-inference-server with Llama-3.3-70B-Instruct, then retry:"
            echo "    ./demos/record-all.sh ${name}"
            echo ""
            return 1
        fi
        echo "  LLM server: OK"
    fi

    # Start hardware recording in background.
    asciinema rec --overwrite \
            --cols "$HW_COLS" --rows "$HW_ROWS" \
            -t "$hw_title" \
            -c "$HW_CMD" \
            "$hw_cast" \
        >/dev/null 2>&1 &
    local hw_pid=$!

    echo "  Waiting ${HW_HEAD}s for hardware visualization to initialize..."
    sleep "$HW_HEAD"

    # Record the demo in foreground at the demo-specific terminal size.
    echo "  Recording demo..."
    asciinema rec --overwrite \
            --cols "$cols" --rows "$rows" \
            -t "$main_title" \
            -c "bash ${script}" \
            "$main_cast"

    echo "  Demo done. Holding hardware view for ${HW_TAIL}s..."
    sleep "$HW_TAIL"

    kill -TERM "$hw_pid" 2>/dev/null || true
    wait "$hw_pid" 2>/dev/null || true

    local main_size hw_size
    main_size=$(wc -c < "$main_cast" 2>/dev/null || echo "?")
    hw_size=$(wc -c < "$hw_cast" 2>/dev/null || echo "?")
    echo "  Saved: ${main_cast} (${main_size} bytes)"
    echo "  Saved: ${hw_cast} (${hw_size} bytes)"
}

# ── Main ──────────────────────────────────────────────────────────────────────

echo ""
echo "╔══════════════════════════════════════════════════════"
echo "║  Zork on Tenstorrent — recording demos"
echo "║  Each demo: game cast + hardware visualization cast"
echo "╚══════════════════════════════════════════════════════"

for target in "${TARGETS[@]}"; do
    record_demo "$target"
done

echo ""
echo "╔══════════════════════════════════════════════════════"
echo "║  All done. Cast files:"
echo "║"
for name in "${TARGETS[@]}"; do
    printf "║  %-8s game: demos/%s.cast\n" "$name" "$name"
    printf "║  %-8s hw:   demos/%s-hw.cast\n" "$name" "$name"
done
echo "║"
echo "║  Playback:"
echo "║    asciinema play demos/ai.cast"
echo "║    asciinema play demos/hybrid.cast"
echo "╚══════════════════════════════════════════════════════"
