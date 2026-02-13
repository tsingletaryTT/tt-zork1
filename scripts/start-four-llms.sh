#!/bin/bash
# Start 4-LLM Zork Architecture
# Each Qwen3-0.6B instance runs on a dedicated P300C chip

set -e

echo "🚀 Starting 4-LLM Zork Architecture..."
echo ""

# Parse arguments
SKIP_WAIT=false
if [[ "$1" == "--no-wait" ]]; then
  SKIP_WAIT=true
fi

# Ensure correct stack (single-chip mode)
echo "Configuring stack for single-chip deployment..."
tt stack use qwen3-0.6b-p100 2>/dev/null || true
echo ""

# Start servers (one per chip)
echo "🔧 Starting LLM servers..."
echo "   (Each server takes ~2-3 minutes to fully initialize)"
echo ""

echo "[1/4] Starting Command Translator (Device 0, Port 8000)..."
tt serve start Qwen3-0.6B --device-id 0 --port 8000 --detach
echo "      Waiting 15s before starting next server..."
sleep 15

echo "[2/4] Starting ASCII Artist (Device 1, Port 8001)..."
tt serve start Qwen3-0.6B --device-id 1 --port 8001 --detach
echo "      Waiting 15s before starting next server..."
sleep 15

echo "[3/4] Starting Dungeon Master (Device 2, Port 8002)..."
tt serve start Qwen3-0.6B --device-id 2 --port 8002 --detach
echo "      Waiting 15s before starting next server..."
sleep 15

echo "[4/4] Starting AI Player (Device 3, Port 8003)..."
tt serve start Qwen3-0.6B --device-id 3 --port 8003 --detach
echo ""

if [ "$SKIP_WAIT" = true ]; then
  echo "⚠️  --no-wait specified, skipping health checks"
  echo "   Check server status manually with: tt serve status"
  echo "   Or run: ./scripts/check-server-health.sh"
  exit 0
fi

echo "⏳ Waiting for all servers to initialize..."
echo "   This typically takes 2-5 minutes per server on first run"
echo "   (Subsequent runs are faster due to cached compilation)"
echo ""
echo "   Progress indicators:"
echo "   - Model loading: ~30s"
echo "   - Kernel compilation: ~60-120s (first run only)"
echo "   - Warmup: ~30s"
echo ""

# Parallel health check function
check_server_health() {
  local port=$1
  local name=$2
  local timeout=600  # 10 minutes max per server
  local interval=10
  local elapsed=0

  while [ $elapsed -lt $timeout ]; do
    if curl -s --max-time 2 http://localhost:$port/health > /dev/null 2>&1; then
      echo "[$name] ✅ Ready! (${elapsed}s)"
      return 0
    fi
    sleep $interval
    elapsed=$((elapsed + interval))
  done

  echo "[$name] ❌ Timeout after ${timeout}s"
  return 1
}

# Start parallel health checks in background
echo "Checking all servers in parallel..."
echo ""

check_server_health 8000 "Translator (Device 0)" > /tmp/llm_check_8000.log 2>&1 &
PID_8000=$!

check_server_health 8001 "Artist (Device 1)" > /tmp/llm_check_8001.log 2>&1 &
PID_8001=$!

check_server_health 8002 "DM (Device 2)" > /tmp/llm_check_8002.log 2>&1 &
PID_8002=$!

check_server_health 8003 "Player (Device 3)" > /tmp/llm_check_8003.log 2>&1 &
PID_8003=$!

# Show progress while waiting
echo "Monitoring startup progress (press Ctrl+C to skip waiting)..."
echo ""

PROGRESS_INTERVAL=10
MAX_WAIT=600  # 10 minutes total
TOTAL_ELAPSED=0

while [ $TOTAL_ELAPSED -lt $MAX_WAIT ]; do
  # Check if all background jobs finished
  JOBS_RUNNING=0

  if ps -p $PID_8000 > /dev/null 2>&1; then JOBS_RUNNING=$((JOBS_RUNNING + 1)); fi
  if ps -p $PID_8001 > /dev/null 2>&1; then JOBS_RUNNING=$((JOBS_RUNNING + 1)); fi
  if ps -p $PID_8002 > /dev/null 2>&1; then JOBS_RUNNING=$((JOBS_RUNNING + 1)); fi
  if ps -p $PID_8003 > /dev/null 2>&1; then JOBS_RUNNING=$((JOBS_RUNNING + 1)); fi

  if [ $JOBS_RUNNING -eq 0 ]; then
    # All checks completed
    break
  fi

  # Show progress
  echo "[${TOTAL_ELAPSED}s] Still waiting for $JOBS_RUNNING server(s)..."

  sleep $PROGRESS_INTERVAL
  TOTAL_ELAPSED=$((TOTAL_ELAPSED + PROGRESS_INTERVAL))
done

# Wait for all background jobs to complete
wait $PID_8000 2>/dev/null; STATUS_8000=$?
wait $PID_8001 2>/dev/null; STATUS_8001=$?
wait $PID_8002 2>/dev/null; STATUS_8002=$?
wait $PID_8003 2>/dev/null; STATUS_8003=$?

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📊 Server Startup Results:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Display results from logs
cat /tmp/llm_check_8000.log
cat /tmp/llm_check_8001.log
cat /tmp/llm_check_8002.log
cat /tmp/llm_check_8003.log

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Count successes
SUCCESS_COUNT=0
if [ $STATUS_8000 -eq 0 ]; then SUCCESS_COUNT=$((SUCCESS_COUNT + 1)); fi
if [ $STATUS_8001 -eq 0 ]; then SUCCESS_COUNT=$((SUCCESS_COUNT + 1)); fi
if [ $STATUS_8002 -eq 0 ]; then SUCCESS_COUNT=$((SUCCESS_COUNT + 1)); fi
if [ $STATUS_8003 -eq 0 ]; then SUCCESS_COUNT=$((SUCCESS_COUNT + 1)); fi

echo "✅ $SUCCESS_COUNT/4 servers started successfully"
echo ""

if [ $SUCCESS_COUNT -lt 4 ]; then
  echo "⚠️  Some servers failed to start. Troubleshooting:"
  echo ""
  echo "   1. Check server logs:"
  echo "      ls -ltr ~/.tt/servers/logs/"
  echo "      tail -100 ~/.tt/servers/logs/qwen3-0.6b.log"
  echo ""
  echo "   2. Check for stuck processes:"
  echo "      ps aux | grep vllm"
  echo ""
  echo "   3. Try restarting failed servers individually:"
  if [ $STATUS_8000 -ne 0 ]; then
    echo "      tt serve start Qwen3-0.6B --device-id 0 --port 8000 --detach"
  fi
  if [ $STATUS_8001 -ne 0 ]; then
    echo "      tt serve start Qwen3-0.6B --device-id 1 --port 8001 --detach"
  fi
  if [ $STATUS_8002 -ne 0 ]; then
    echo "      tt serve start Qwen3-0.6B --device-id 2 --port 8002 --detach"
  fi
  if [ $STATUS_8003 -ne 0 ]; then
    echo "      tt serve start Qwen3-0.6B --device-id 3 --port 8003 --detach"
  fi
  echo ""
  echo "   4. View detailed status:"
  echo "      tt serve status"
  echo ""
fi

# Show server status regardless of health check results
echo "Current server status:"
echo ""
tt serve status 2>/dev/null || echo "  (tt serve status not available)"
echo ""

if [ $SUCCESS_COUNT -eq 4 ]; then
  echo "🎉 All servers ready!"
  echo ""
  echo "Endpoints:"
  echo "  • Command Translator: http://localhost:8000"
  echo "  • ASCII Artist:       http://localhost:8001"
  echo "  • Dungeon Master:     http://localhost:8002"
  echo "  • AI Player:          http://localhost:8003"
  echo ""
  echo "Next steps:"
  echo "  • Test endpoints:     ./scripts/test-four-llms.sh"
  echo "  • Test prompts:       ./scripts/test-prompt-variants.sh"
  echo "  • Tune interactively: ./scripts/tune-prompt-interactive.sh"
  echo ""
else
  echo "You can still test working servers with:"
  echo "  ./scripts/test-four-llms.sh"
  echo ""
fi

echo "To stop all servers:"
echo "  ./scripts/stop-four-llms.sh"
echo ""

# Cleanup temp files
rm -f /tmp/llm_check_*.log
