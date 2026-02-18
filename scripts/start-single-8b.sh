#!/bin/bash
# Start Single 8B Model with Tensor Parallelism
# Routes all 4 tasks (Translator, Artist, DM, Player) to one model

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "🚀 Starting Single 8B Model with Tensor Parallelism..."
echo ""

# Configuration
MODEL="meta-llama/Llama-3.1-8B-Instruct"
PORT=9000
DEVICES="0,1,2,3"  # All 4 P300C chips
TENSOR_PARALLEL_SIZE=4

# Check if already running
if curl -s --max-time 2 http://localhost:$PORT/health > /dev/null 2>&1; then
  echo "✅ 8B model already running on port $PORT"
  echo ""
  echo "Endpoint: http://localhost:$PORT"
  echo "Model: $MODEL"
  echo "Tensor Parallel: $TENSOR_PARALLEL_SIZE chips"
  echo ""
  echo "To stop: ./scripts/stop-single-8b.sh"
  exit 0
fi

echo "📋 Configuration:"
echo "  Model:            $MODEL"
echo "  Port:             $PORT"
echo "  Devices:          $DEVICES"
echo "  Tensor Parallel:  $TENSOR_PARALLEL_SIZE"
echo "  Max Context:      4096 tokens"
echo ""

# Check if any other servers are running
if tt serve status 2>/dev/null | grep -q "Port:"; then
  echo "⚠️  WARNING: Other TT-serve instances detected!"
  echo ""
  tt serve status
  echo ""
  read -p "Stop all servers before continuing? (y/N) " -n 1 -r
  echo
  if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Stopping all servers..."
    tt serve stop --all
    sleep 5
  else
    echo "Continuing with existing servers..."
  fi
fi

echo "🔧 Starting 8B model (this takes ~3-5 minutes)..."
echo "   (Distributing across 4 chips with tensor parallelism)"
echo ""

# Start server with tensor parallelism
# Note: --tensor-parallel-size must equal number of device IDs
tt serve start "$MODEL" \
  --device-ids "$DEVICES" \
  --tensor-parallel-size "$TENSOR_PARALLEL_SIZE" \
  --port "$PORT" \
  --max-model-len 4096 \
  --max-num-seqs 16 \
  --detach

echo ""
echo "⏳ Waiting for model to initialize..."
echo "   Progress indicators:"
echo "   • Compiling kernels (1-2 min)"
echo "   • Loading weights (1-2 min)"
echo "   • Initializing inference (30s)"
echo ""

# Wait for health check with progress
TIMEOUT=600  # 10 minutes
INTERVAL=10
ELAPSED=0

while [ $ELAPSED -lt $TIMEOUT ]; do
  if curl -s --max-time 2 http://localhost:$PORT/health > /dev/null 2>&1; then
    echo ""
    echo "✅ Model ready! (${ELAPSED}s)"
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "🎉 Single 8B Model Running!"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    echo "Endpoint:  http://localhost:$PORT"
    echo "Model:     $MODEL"
    echo "Mode:      Tensor Parallel across 4 chips"
    echo ""
    echo "All 4 tasks (Translator, Artist, DM, Player) will use this model"
    echo ""
    echo "Next steps:"
    echo "  • Test endpoint:  curl http://localhost:$PORT/health"
    echo "  • Play Zork:      ZORK_8B_MODE=1 ./play-zork-with-llm.sh"
    echo "  • Stop server:    ./scripts/stop-single-8b.sh"
    echo ""
    exit 0
  fi

  echo "[${ELAPSED}s] Still initializing..."
  sleep $INTERVAL
  ELAPSED=$((ELAPSED + INTERVAL))
done

echo ""
echo "❌ Timeout after ${TIMEOUT}s"
echo ""
echo "Troubleshooting:"
echo "  1. Check logs: ls -ltr ~/.tt/servers/logs/"
echo "  2. Check status: tt serve status"
echo "  3. Try stopping and restarting: tt serve stop --all"
echo ""
exit 1
