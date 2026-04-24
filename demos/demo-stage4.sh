#!/usr/bin/env bash
# demo-stage4.sh — Stage 4: LLM Remix layer — the punchline
# Record with: asciinema rec -t "Zork on Tenstorrent — Stage 4" demos/stage4.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

# LLM backend: tt-inference-server running Llama on Tensix cores.
# Override ZORK_LLM_URL to point at Ollama or any OpenAI-compatible server.
export ZORK_LLM_URL="${ZORK_LLM_URL:-http://localhost:8001/v1/chat/completions}"
export ZORK_LLM_MODEL="${ZORK_LLM_MODEL:-meta-llama/Llama-3.1-8B-Instruct}"

echo ""
echo "╔══════════════════════════════════════════════════════════════"
echo "║  STAGE 4 — LLM Remix: Never Be Told NO Again"
echo "╚══════════════════════════════════════════════════════════════"
echo ""
sleep 2
echo "  Starting in CLASSIC mode — then switching to REMIX live."
echo ""
sleep 3

{
  sleep 5
  echo "/classic"        # confirm we start classic
  sleep 2
  echo "open mailbox"    # classic: raw Z-machine response
  sleep 3
  echo "/remix"          # switch to remix mode LIVE
  sleep 2
  echo "open the mailbox with my teeth"  # remix: creative response
  sleep 5
  echo "eat the leaflet"                 # something Zork wouldn't allow
  sleep 5
  echo "fight the darkness with hope"    # poetic nonsense — LLM handles it
  sleep 5
  echo "quit"
} | python play.py --stage sim --remix --game game/zork1.z3
