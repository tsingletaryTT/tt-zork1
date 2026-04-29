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
#   hybrid: Tensix cores hot  (Llama-3.3-70B inference rewrites each response)
#   ai:     Tensix cores hot  (Llama-3.3-70B plays 30 turns end-to-end)
#
# Usage:
#   ./demos/record-all.sh                   # all four demos in order
#   ./demos/record-all.sh stage1            # just Stage 1
#   ./demos/record-all.sh stage2 stage3     # Stages 2 and 3
#   ./demos/record-all.sh hybrid ai         # the two LLM demos
#
# Requirements:
#   sudo apt install asciinema
#   source ~/code/tt-lang/build/env/activate   (done by each demo script)
#
# hybrid and ai:
#   The inference server starts automatically after stage2 resets the hardware.
#   To run hybrid/ai standalone (server already up):
#     curl -s http://localhost:8000/v1/models   # verify it's ready first

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
    "hybrid|demos/demo-hybrid.sh|120|35|yes"
    "ai|demos/demo-ai.sh|160|40|yes"
)

# ── Stage list ────────────────────────────────────────────────────────────────

if [[ $# -eq 0 ]]; then
    TARGETS=(stage1 stage2 hybrid ai)
else
    TARGETS=("$@")
fi

# ── Inference server setup ────────────────────────────────────────────────────

_llm_server_started=0  # track so we only start it once per run

start_inference_server() {
    if [[ $_llm_server_started -eq 1 ]]; then
        return 0
    fi

    echo ""
    echo "╔══════════════════════════════════════════════════════"
    echo "║  Setting up LLM inference server"
    echo "╚══════════════════════════════════════════════════════"

    # Start the inference server using the known-good deployment script.
    # ~/tt-home/70b-up.sh already handles: docker stop, tt-smi -r, sleep 5.
    local log="/tmp/tt-inference-server-demo.log"
    echo "  Starting Llama-3.3-70B via ~/tt-home/70b-up.sh..."
    echo "  Logs: ${log}"
    bash ~/tt-home/70b-up.sh > "$log" 2>&1 &

    # Wait for the server's distinct ready marker in the log.
    # tt-inference-server prints exactly this line when the model is warm:
    #   "All devices are warmed up and ready"
    local ready_marker="All devices are warmed up and ready"
    local timeout=360
    local elapsed=0
    local interval=5
    echo "  Waiting for '${ready_marker}' in logs (up to ${timeout}s)..."
    while [[ $elapsed -lt $timeout ]]; do
        if grep -qF "$ready_marker" "$log" 2>/dev/null; then
            echo "  Inference server ready! (${elapsed}s)"
            _llm_server_started=1
            return 0
        fi
        # Also check that the background process is still alive.
        if ! jobs %% >/dev/null 2>&1; then
            echo ""
            echo "  ERROR: Inference server process exited unexpectedly."
            echo "  Check logs: ${log}"
            tail -20 "$log" 2>/dev/null || true
            exit 1
        fi
        sleep "$interval"
        elapsed=$((elapsed + interval))
        if (( elapsed % 30 == 0 )); then
            echo "  Still waiting... (${elapsed}s elapsed)"
        fi
    done

    echo ""
    echo "  ERROR: Inference server did not become ready within ${timeout}s."
    echo "  Check logs: ${log}"
    tail -20 "$log" 2>/dev/null || true
    exit 1
}

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
        echo "Unknown demo: ${name}. Valid names: stage1 stage2 hybrid ai"
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
        if ! curl -sf --max-time 5 "${LLM_URL%/chat/completions}/models" >/dev/null 2>&1; then
            echo "  LLM server not running — starting automatically..."
            start_inference_server
        else
            echo "  LLM server: already running"
        fi
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
