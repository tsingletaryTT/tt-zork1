#!/usr/bin/env bash
# demo-hybrid.sh — LLM remix layer: every response rewritten by Llama-3.3-70B
#
# This demo shows hybrid mode: the Z-machine handles all game logic; the LLM
# rewrites each response in a richer, more atmospheric voice. The human still
# drives — the AI just makes the prose more alive.
#
# Prerequisite: tt-inference-server serving Llama-3.3-70B-Instruct on :8000
#   curl -s http://localhost:8000/v1/models   # verify it's ready
#
# Record with:
#   asciinema rec -t "Zork × Llama — Hybrid Mode" demos/hybrid.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

export ZORK_LLM_MODEL="${ZORK_LLM_MODEL:-meta-llama/Llama-3.3-70B-Instruct}"
export ZORK_LLM_URL="${ZORK_LLM_URL:-http://localhost:8000/v1/chat/completions}"

echo ""
echo "╔══════════════════════════════════════════════════════════════"
echo "║  HYBRID MODE — LLM remix layer active"
echo "║  Model: ${ZORK_LLM_MODEL}"
echo "║  Every Z-machine response is rewritten by the LLM."
echo "╚══════════════════════════════════════════════════════════════"
echo ""
sleep 3

{
  sleep 5       # opening text + first remix
  echo "look"
  sleep 12      # LLM rewrites the room description
  echo "open mailbox"
  sleep 12      # LLM rewrites the mailbox response
  echo "take leaflet"
  sleep 10
  echo "read leaflet"
  sleep 15      # longer — leaflet is multi-paragraph
  echo "go north"
  sleep 12
  echo "quit"
} | python play.py --stage sim --game game/zork1.z3 --remix
