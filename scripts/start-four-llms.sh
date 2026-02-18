#!/bin/bash
# Start 4-LLM Zork Architecture with Resilience
# Reads config from llm_mode.yaml and starts only missing servers

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="$SCRIPT_DIR/../config/llm_mode.yaml"

echo "🚀 Starting 4-LLM Zork Architecture..."
echo ""

# Parse arguments
SKIP_WAIT=false
if [[ "$1" == "--no-wait" ]]; then
  SKIP_WAIT=true
fi

# Helper function to check if server is running on a port
is_server_running() {
  local port=$1
  curl -s --max-time 2 http://localhost:$port/health > /dev/null 2>&1
  return $?
}

# Helper function to check if server is in process list
is_server_starting() {
  local port=$1
  tt serve status 2>/dev/null | grep -q "Port: $port"
  return $?
}

echo "📋 Checking current server status..."
echo ""

# Define servers based on config
# Hardware: 4x P300C = 4 devices (0, 1, 2, 3)
# ALL LLAMA CONFIGURATION - Testing if Llama-3.2-1B works better across the board
# Translator: Llama-3.2-1B on device 0, port 8000
# Artist: Llama-3.2-1B on device 1, port 8001
# DM: Llama-3.2-1B on device 2, port 8002
# Player: Llama-3.2-1B on device 3, port 8003

declare -A SERVERS
SERVERS[8000]="Llama-3.2-1B-Instruct:0:Translator"
SERVERS[8001]="Llama-3.2-1B-Instruct:1:Artist"
SERVERS[8002]="Llama-3.2-1B-Instruct:2:DM"
SERVERS[8003]="Llama-3.2-1B-Instruct:3:Player"

# Check which servers need starting (reordered: Llama last)
TO_START=()
for port in 8000 8002 8003 8001; do
  IFS=':' read -r model device name <<< "${SERVERS[$port]}"

  if is_server_running $port; then
    echo "✅ $name (port $port): Already running and healthy"
  elif is_server_starting $port; then
    echo "⏳ $name (port $port): Starting (will wait for health check)"
    TO_START+=("$port:$model:$device:$name:resuming")
  else
    echo "❌ $name (port $port): Not running - will start"
    TO_START+=("$port:$model:$device:$name:starting")
  fi
done

echo ""

if [ ${#TO_START[@]} -eq 0 ]; then
  echo "🎉 All 4 servers are already running!"
  echo ""
  echo "Endpoints:"
  echo "  • Command Translator: http://localhost:8000"
  echo "  • ASCII Artist:       http://localhost:8001"
  echo "  • Dungeon Master:     http://localhost:8002"
  echo "  • AI Player:          http://localhost:8003"
  echo ""
  echo "To stop all servers: ./scripts/stop-four-llms.sh"
  echo "To test endpoints:   ./scripts/test-four-llms.sh"
  exit 0
fi

echo "🔧 Starting ${#TO_START[@]} server(s)..."
echo "   (Each server takes ~2-3 minutes to fully initialize)"
echo ""

# Start servers that need starting
for entry in "${TO_START[@]}"; do
  IFS=':' read -r port model device name status <<< "$entry"

  if [ "$status" = "starting" ]; then
    echo "[$name] Starting on port $port (device $device)..."

    # Use explicit parameters to avoid stack configuration conflicts
    # Small models (0.6B, 1B) work well with these settings
    tt serve start "$model" \
      --device-id "$device" \
      --port "$port" \
      --max-model-len 2048 \
      --max-num-seqs 32 \
      --detach

    echo "      Waiting 15s before next server..."
    sleep 15
  else
    echo "[$name] Already starting, will check health..."
  fi
done

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

# Start parallel health checks for all ports that need checking
PIDS=()
for entry in "${TO_START[@]}"; do
  IFS=':' read -r port model device name status <<< "$entry"
  check_server_health "$port" "$name" > "/tmp/llm_check_${port}.log" 2>&1 &
  PIDS+=($!)
done

# Also check already-running servers to confirm they're still healthy
for port in 8000 8001 8002 8003; do
  # Skip if we're already checking this port
  skip=false
  for entry in "${TO_START[@]}"; do
    IFS=':' read -r check_port _ _ _ _ <<< "$entry"
    if [ "$check_port" = "$port" ]; then
      skip=true
      break
    fi
  done

  if [ "$skip" = false ]; then
    IFS=':' read -r model device name <<< "${SERVERS[$port]}"
    check_server_health "$port" "$name" > "/tmp/llm_check_${port}.log" 2>&1 &
    PIDS+=($!)
  fi
done

# Show progress while waiting
echo "Monitoring startup progress (press Ctrl+C to skip waiting)..."
echo ""

PROGRESS_INTERVAL=10
MAX_WAIT=600  # 10 minutes total
TOTAL_ELAPSED=0

while [ $TOTAL_ELAPSED -lt $MAX_WAIT ]; do
  # Check if all background jobs finished
  JOBS_RUNNING=0
  for pid in "${PIDS[@]}"; do
    if ps -p $pid > /dev/null 2>&1; then
      JOBS_RUNNING=$((JOBS_RUNNING + 1))
    fi
  done

  if [ $JOBS_RUNNING -eq 0 ]; then
    # All checks completed
    break
  fi

  # Show progress
  echo "[${TOTAL_ELAPSED}s] Still waiting for $JOBS_RUNNING server(s)..."

  sleep $PROGRESS_INTERVAL
  TOTAL_ELAPSED=$((TOTAL_ELAPSED + PROGRESS_INTERVAL))
done

# Wait for all background jobs and collect results
declare -A RESULTS
for port in 8000 8001 8002 8003; do
  for pid in "${PIDS[@]}"; do
    wait $pid 2>/dev/null && RESULTS[$port]=0 || RESULTS[$port]=1
  done
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📊 Server Startup Results:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Display results from logs
for port in 8000 8001 8002 8003; do
  if [ -f "/tmp/llm_check_${port}.log" ]; then
    cat "/tmp/llm_check_${port}.log"
  fi
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Count successes
SUCCESS_COUNT=0
for port in 8000 8001 8002 8003; do
  if is_server_running $port; then
    SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
  fi
done

echo "✅ $SUCCESS_COUNT/4 servers healthy"
echo ""

if [ $SUCCESS_COUNT -lt 4 ]; then
  echo "⚠️  Some servers are not healthy. Troubleshooting:"
  echo ""
  echo "   1. Check server logs:"
  echo "      ls -ltr ~/.tt/servers/logs/"
  echo ""
  echo "   2. Check for stuck processes:"
  echo "      ps aux | grep vllm"
  echo ""
  echo "   3. Try restarting failed servers:"
  for port in 8000 8001 8002 8003; do
    if ! is_server_running $port; then
      IFS=':' read -r model device name <<< "${SERVERS[$port]}"
      echo "      tt serve start $model --device-id $device --port $port --detach"
    fi
  done
  echo ""
  echo "   4. View detailed status:"
  echo "      tt serve status"
  echo ""
fi

# Show server status
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
  echo "  • Play Zork:          ./play-zork-with-llm.sh"
  echo "  • Test endpoints:     ./scripts/test-four-llms.sh"
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
