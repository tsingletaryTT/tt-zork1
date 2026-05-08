#!/usr/bin/env bash
# record-all.sh — Record all demos to asciinema cast files
#
# Usage:
#   ./demos/record-all.sh                   # all four demos in order
#   ./demos/record-all.sh stage1            # just Stage 1
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

LLM_URL="${ZORK_LLM_URL:-http://localhost:8000/v1/chat/completions}"

# ── Demo registry ─────────────────────────────────────────────────────────────
# Each entry: "name|script|cols|rows|needs_llm"
# ai and zork2-ai use 160x40 to give the TUI enough room.

DEMOS=(
    "stage1|demos/demo-stage1.sh|120|35|no"
    "stage2|demos/demo-stage2.sh|120|35|no"
    "hybrid|demos/demo-hybrid.sh|120|35|yes"
    "ai|demos/demo-ai.sh|160|40|yes"
    "zork2-stage2|demos/demo-zork2-stage2.sh|120|35|no"
    "zork2-ai|demos/demo-zork2-ai.sh|160|40|yes"
    "advent|demos/demo-advent.sh|120|35|no"
)

# ── Stage list ────────────────────────────────────────────────────────────────

if [[ $# -eq 0 ]]; then
    TARGETS=(stage1 stage2 hybrid ai zork2-stage2 zork2-ai advent)
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
        echo "Unknown demo: ${name}. Valid names: stage1 stage2 hybrid ai zork2-stage2 zork2-ai"
        return 1
    fi

    IFS='|' read -r _ script cols rows needs_llm <<< "$entry"

    local cast="demos/${name}.cast"
    local title="Zork on Tenstorrent — ${name}"

    echo ""
    echo "╔══════════════════════════════════════════════════════"
    echo "║  Demo: ${name}"
    echo "║  Cast: ${cast}"
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

    # Record the demo.
    echo "  Recording..."
    asciinema rec --overwrite \
            --cols "$cols" --rows "$rows" \
            -t "$title" \
            -c "bash ${script}" \
            "$cast"

    local size
    size=$(wc -c < "$cast" 2>/dev/null || echo "?")
    echo "  Saved: ${cast} (${size} bytes)"
}

# ── Main ──────────────────────────────────────────────────────────────────────

echo ""
echo "╔══════════════════════════════════════════════════════"
echo "║  Zork on Tenstorrent — recording demos"
echo "╚══════════════════════════════════════════════════════"

for target in "${TARGETS[@]}"; do
    record_demo "$target"
done

echo ""
echo "╔══════════════════════════════════════════════════════"
echo "║  All done. Cast files:"
echo "║"
for name in "${TARGETS[@]}"; do
    printf "║    demos/%s.cast\n" "$name"
done
echo "║"
echo "║  Playback:"
echo "║    asciinema play demos/ai.cast"
echo "║    asciinema play demos/hybrid.cast"
echo "╚══════════════════════════════════════════════════════"
