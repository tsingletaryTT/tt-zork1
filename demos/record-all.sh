#!/usr/bin/env bash
# record-all.sh — Record all stage demos + simultaneous hardware visualization
#
# For each stage this script records two cast files in parallel:
#   stageN.cast    — the demo itself (game play, output, narrative)
#   stageN-hw.cast — tt-toplike arcade mode running throughout
#
# The hw cast tells the hardware story:
#   Stage 1: chip mostly idle  (pure Python, no hardware used)
#   Stage 2: DRAM activity     (86 KB game file lives on-chip DRAM)
#   Stage 3: RISC-V cores lit  (Z-machine interpreter on Blackhole silicon)
#   Stage 4: Tensix cores hot  (Llama-3.1-8B inference on Tensix fabric)
#
# The hw recording also doubles as an end-to-end wall-clock timer for each stage.
#
# Usage:
#   ./demos/record-all.sh         # all four stages in order
#   ./demos/record-all.sh 1       # just Stage 1
#   ./demos/record-all.sh 2 3     # Stages 2 and 3
#   ./demos/record-all.sh 4       # Stage 4 (requires LLM server on :8001)
#
# Requirements:
#   sudo apt install asciinema
#   source ~/code/tt-lang/build/env/activate   (already done by each demo script)
#
# Stage 4 extra requirement:
#   tt-inference-server must be running on port 8001 before this script starts.
#   export ZORK_LLM_URL=http://localhost:8001/v1/chat/completions
#   Quick check: curl -s http://localhost:8001/health
#
# After recording:
#   asciinema play demos/stage4.cast
#   asciinema play demos/stage4-hw.cast

set -euo pipefail
cd "$(dirname "$0")/.."

# ── Configuration ─────────────────────────────────────────────────────────────

# --json backend: polls tt-smi subprocess — non-invasive, does NOT hold the
# Tenstorrent device lock, so Stages 2/3 can open the device via ttnn in parallel.
HW_CMD="tt-toplike -m arcade --json -q"

# Terminal size for the hw cast — wide enough for arcade mode to look good.
HW_COLS=160
HW_ROWS=42

# Seconds to let tt-toplike render before starting the demo (one full frame).
HW_HEAD=2

# Seconds of hw recording to keep after the demo finishes (shows final state).
HW_TAIL=4

# LLM server URL — Stage 4 pre-flight check.
LLM_URL="${ZORK_LLM_URL:-http://localhost:8001/v1/chat/completions}"

# ── Stage list ────────────────────────────────────────────────────────────────

STAGES="${*:-1 2 3 4}"

# ── Helper: record one stage ──────────────────────────────────────────────────

record_stage() {
    local n="$1"
    local demo_script="demos/demo-stage${n}.sh"
    local main_cast="demos/stage${n}.cast"
    local hw_cast="demos/stage${n}-hw.cast"
    local main_title="Zork on Tenstorrent — Stage ${n}"
    local hw_title="Zork on Tenstorrent — Stage ${n} hardware"

    echo ""
    echo "══════════════════════════════════════════════════════"
    echo "  Stage ${n}"
    echo "  Game cast:  ${main_cast}"
    echo "  HW cast:    ${hw_cast}"
    echo "══════════════════════════════════════════════════════"

    # Pre-flight: Stage 4 needs the LLM server.
    if [[ "$n" == "4" ]]; then
        echo "  Checking LLM server at ${LLM_URL} ..."
        if ! curl -sf --max-time 5 "${LLM_URL%/chat/completions}/models" >/dev/null 2>&1; then
            echo ""
            echo "  ERROR: LLM server not reachable at ${LLM_URL}"
            echo "  Start tt-inference-server first, then re-run:"
            echo "    ./demos/record-all.sh 4"
            echo ""
            return 1
        fi
        echo "  LLM server: OK"
    fi

    # ── Start hardware recording in the background ─────────────────────────
    # Redirect stdout/stderr so asciinema's own messages don't bleed into the
    # foreground terminal while the demo is recording.
    COLUMNS=$HW_COLS LINES=$HW_ROWS \
        asciinema rec --overwrite \
            -t "$hw_title" \
            -c "$HW_CMD" \
            "$hw_cast" \
        >/dev/null 2>&1 &
    local hw_pid=$!

    # Give tt-toplike time to render at least one complete frame before the
    # demo starts — otherwise the hw cast opens on a blank screen.
    echo "  Waiting ${HW_HEAD}s for hardware visualization to initialize..."
    sleep "$HW_HEAD"

    # ── Record the main demo in the foreground ────────────────────────────
    echo "  Recording demo..."
    asciinema rec --overwrite \
        -t "$main_title" \
        -c "bash $demo_script" \
        "$main_cast"

    # ── Keep hw recording alive briefly so final state is visible ─────────
    echo "  Demo done. Holding hardware view for ${HW_TAIL}s..."
    sleep "$HW_TAIL"

    # ── Stop hw recording cleanly ─────────────────────────────────────────
    # SIGTERM → asciinema finalises and saves the cast file before exiting.
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
echo "║  Zork on Tenstorrent — recording all stages"
echo "║  Each stage: game cast + hardware visualization cast"
echo "╚══════════════════════════════════════════════════════"

for stage in $STAGES; do
    record_stage "$stage"
done

echo ""
echo "══════════════════════════════════════════════════════"
echo "  All done. Cast files:"
echo ""
for n in $STAGES; do
    printf "  Stage %s game: %s\n" "$n" "demos/stage${n}.cast"
    printf "  Stage %s hw:   %s\n" "$n" "demos/stage${n}-hw.cast"
done
echo ""
echo "  Playback:"
echo "    asciinema play demos/stage4.cast"
echo "    asciinema play demos/stage4-hw.cast"
echo "══════════════════════════════════════════════════════"
