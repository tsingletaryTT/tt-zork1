#!/bin/bash
# Stop all 4 LLM servers with resilience
# Handles 4 devices (0, 1, 2, 3) with different models

echo "🛑 Stopping 4-LLM Architecture..."
echo ""

# Helper function to check if server is running on a port
is_server_running() {
  local port=$1
  tt serve status 2>/dev/null | grep -q "Port: $port"
  return $?
}

# Helper function to stop server by model and device
stop_server() {
  local model=$1
  local device=$2
  local name=$3
  local port=$4

  echo "Stopping $name ($model on device $device, port $port)..."

  # Try to stop via tt command
  if tt serve status 2>/dev/null | grep -A3 "Port: $port" > /dev/null; then
    tt stop "$model" --device-id "$device" 2>/dev/null || true
    sleep 1

    # Verify stopped
    if is_server_running $port; then
      # Force kill by PID
      PID=$(tt serve status 2>/dev/null | grep -A2 "Port: $port" | grep "PID:" | awk '{print $2}')
      if [ -n "$PID" ]; then
        kill -9 $PID 2>/dev/null || true
        echo "  ✓ Force stopped (PID $PID)"
      fi
    else
      echo "  ✓ Stopped"
    fi
  else
    echo "  (not running)"
  fi
}

# Stop all servers (based on current configuration)
stop_server "Qwen3-0.6B" 0 "Translator" 8000
stop_server "Llama-3.2-1B-Instruct" 1 "Artist" 8001
stop_server "Qwen3-0.6B" 2 "DM" 8002
stop_server "Qwen3-0.6B" 3 "Player" 8003

echo ""

# Double-check and kill any remaining vllm processes
REMAINING=$(ps aux | grep -E 'vllm.*(:8000|:8001|:8002|:8003)' | grep -v grep | wc -l)
if [ "$REMAINING" -gt 0 ]; then
  echo "⚠️  Found $REMAINING remaining vllm process(es), force stopping..."
  ps aux | grep -E 'vllm.*(:8000|:8001|:8002|:8003)' | grep -v grep | awk '{print $2}' | xargs -r kill -9 2>/dev/null || true
  sleep 1
fi

echo "✅ All servers stopped"
echo ""

# Show remaining servers (should be empty)
echo "Remaining servers:"
REMAINING_SERVERS=$(tt serve status 2>/dev/null)
if [ -z "$REMAINING_SERVERS" ] || echo "$REMAINING_SERVERS" | grep -q "No"; then
  echo "  (none - all clean!)"
else
  echo "$REMAINING_SERVERS"
fi
echo ""
