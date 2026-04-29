#!/usr/bin/env bash
# demo-ai.sh — AI auto-play: Llama-3.3-70B plays Zork in experimental mode
#
# The AI takes every turn. The experimental persona tries unusual commands,
# pushes edges, ignores obvious moves. The remix layer rewrites every response
# in an atmospheric voice. The TUI shows streaming tokens and ASCII room art
# as the model generates them.
#
# This is the showpiece. Let it run.
#
# Prerequisite: tt-inference-server serving Llama-3.3-70B-Instruct on :8000
#   curl -s http://localhost:8000/v1/models   # verify it's ready
#
# Record with:
#   COLUMNS=160 LINES=40 \
#     asciinema rec -t "Zork × Llama — AI Player" demos/ai.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

export ZORK_LLM_MODEL="${ZORK_LLM_MODEL:-meta-llama/Llama-3.3-70B-Instruct}"
export ZORK_LLM_URL="${ZORK_LLM_URL:-http://localhost:8000/v1/chat/completions}"

# Pre-flight: confirm the LLM server is reachable before launching the TUI.
if ! curl -sf --max-time 5 "${ZORK_LLM_URL%/chat/completions}/models" >/dev/null 2>&1; then
    echo ""
    echo "ERROR: LLM server not reachable at ${ZORK_LLM_URL}"
    echo "Start tt-inference-server with Llama-3.3-70B-Instruct, then retry."
    exit 1
fi

echo ""
echo "╔══════════════════════════════════════════════════════════════"
echo "║  ZORK I × LLAMA 3.3 70B — AI PLAYER"
echo "║  Persona: experimental  |  Turns: 30  |  Full TUI"
echo "║  The model plays. You watch."
echo "╚══════════════════════════════════════════════════════════════"
echo ""
sleep 3

python play.py \
    --stage sim \
    --game game/zork1.z3 \
    --remix \
    --tui \
    --persona experimental \
    --turns 30
