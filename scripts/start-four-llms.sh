#!/bin/bash
# Start 4-LLM Zork Architecture
# Each Qwen3-0.6B instance runs on a dedicated P300C chip

set -e

echo "🚀 Starting 4-LLM Zork Architecture..."
echo ""

# Ensure correct stack (single-chip mode)
echo "Configuring stack for single-chip deployment..."
tt stack use qwen3-0.6b-p100
echo ""

# Start servers (one per chip)
echo "🔧 Starting LLM servers..."
echo ""

echo "[1/4] Starting Command Translator (Device 0, Port 8000)..."
tt serve start Qwen3-0.6B --device-id 0 --port 8000 --detach
sleep 2

echo "[2/4] Starting ASCII Artist (Device 1, Port 8001)..."
tt serve start Qwen3-0.6B --device-id 1 --port 8001 --detach
sleep 2

echo "[3/4] Starting Dungeon Master (Device 2, Port 8002)..."
tt serve start Qwen3-0.6B --device-id 2 --port 8002 --detach
sleep 2

echo "[4/4] Starting AI Player (Device 3, Port 8003)..."
tt serve start Qwen3-0.6B --device-id 3 --port 8003 --detach
sleep 2

echo ""
echo "⏳ Waiting for servers to initialize (this will take 2-5 minutes per server)..."
echo "   - Model loading: ~30s per chip"
echo "   - Kernel compilation: ~60-120s per chip (first run only)"
echo "   - Warmup: ~30s per chip"
echo ""

# Health check with timeout
TIMEOUT=600  # 10 minutes total
ELAPSED=0
INTERVAL=10

for port in 8000 8001 8002 8003; do
  echo -n "Checking port $port..."

  STARTED=0
  PORT_ELAPSED=0

  while [ $PORT_ELAPSED -lt $TIMEOUT ]; do
    if curl -s --max-time 2 http://localhost:$port/health > /dev/null 2>&1; then
      echo " ✅ Ready! (${PORT_ELAPSED}s)"
      STARTED=1
      break
    fi

    echo -n "."
    sleep $INTERVAL
    PORT_ELAPSED=$((PORT_ELAPSED + INTERVAL))
  done

  if [ $STARTED -eq 0 ]; then
    echo " ❌ Timeout after ${TIMEOUT}s"
    echo ""
    echo "⚠️  Server on port $port failed to start within timeout."
    echo "    Check logs: ~/.tt/servers/logs/"
    echo "    You can continue with other servers or restart with:"
    echo "    tt serve start Qwen3-0.6B --device-id <N> --port $port --detach"
    echo ""
  fi
done

echo ""
echo "📊 Server Status:"
echo ""
tt serve status

echo ""
echo "✅ 4-LLM Architecture Ready!"
echo ""
echo "Endpoints:"
echo "  • Command Translator: http://localhost:8000"
echo "  • ASCII Artist:       http://localhost:8001"
echo "  • Dungeon Master:     http://localhost:8002"
echo "  • AI Player:          http://localhost:8003"
echo ""
echo "Configure Zork with:"
echo "  export ZORK_LLM_TRANSLATE_URL='http://localhost:8000/v1/chat/completions'"
echo "  export ZORK_LLM_VISUALIZE_URL='http://localhost:8001/v1/chat/completions'"
echo "  export ZORK_LLM_NARRATE_URL='http://localhost:8002/v1/chat/completions'"
echo "  export ZORK_LLM_AUTOPLAY_URL='http://localhost:8003/v1/chat/completions'"
echo ""
echo "Start game:"
echo "  ./zork-native game/zork1.z3"
echo ""
echo "Stop servers:"
echo "  ./scripts/stop-four-llms.sh"
echo ""
