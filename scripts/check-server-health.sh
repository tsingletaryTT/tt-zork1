#!/bin/bash
# Check health of all 4 LLM servers independently

echo "🔍 Checking LLM Server Health..."
echo ""

# Function to check a single server
check_one() {
  local port=$1
  local name=$2
  local device=$3

  echo -n "[$name] Port $port (Device $device): "

  # Try health check
  if timeout 3 curl -s http://localhost:$port/health > /dev/null 2>&1; then
    echo "✅ UP"

    # Try to get model info
    if timeout 3 curl -s http://localhost:$port/v1/models > /dev/null 2>&1; then
      echo "           Model endpoint: ✅ Responding"
    else
      echo "           Model endpoint: ⚠️  Not responding"
    fi

    return 0
  else
    echo "❌ DOWN"

    # Check if process exists
    if ps aux | grep -v grep | grep "device-id $device" | grep "port $port" > /dev/null 2>&1; then
      echo "           Process: ⚠️  Running but not responding"
    else
      echo "           Process: ❌ Not running"
    fi

    return 1
  fi
}

# Check all servers in parallel and collect results
check_one 8000 "Translator" 0 &
PID1=$!
check_one 8001 "Artist" 1 &
PID2=$!
check_one 8002 "DM" 2 &
PID3=$!
check_one 8003 "Player" 3 &
PID4=$!

# Wait for all checks
wait $PID1; STATUS1=$?
wait $PID2; STATUS2=$?
wait $PID3; STATUS3=$?
wait $PID4; STATUS4=$?

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Count healthy servers
HEALTHY=0
if [ $STATUS1 -eq 0 ]; then HEALTHY=$((HEALTHY + 1)); fi
if [ $STATUS2 -eq 0 ]; then HEALTHY=$((HEALTHY + 1)); fi
if [ $STATUS3 -eq 0 ]; then HEALTHY=$((HEALTHY + 1)); fi
if [ $STATUS4 -eq 0 ]; then HEALTHY=$((HEALTHY + 1)); fi

echo "Summary: $HEALTHY/4 servers healthy"

if [ $HEALTHY -eq 4 ]; then
  echo "🎉 All systems operational!"
elif [ $HEALTHY -eq 0 ]; then
  echo "⚠️  No servers responding. Start them with:"
  echo "   ./scripts/start-four-llms.sh"
else
  echo "⚠️  Some servers are down. Check logs or restart:"

  if [ $STATUS1 -ne 0 ]; then
    echo "   tt serve start Qwen3-0.6B --device-id 0 --port 8000 --detach"
  fi
  if [ $STATUS2 -ne 0 ]; then
    echo "   tt serve start Qwen3-0.6B --device-id 1 --port 8001 --detach"
  fi
  if [ $STATUS3 -ne 0 ]; then
    echo "   tt serve start Qwen3-0.6B --device-id 2 --port 8002 --detach"
  fi
  if [ $STATUS4 -ne 0 ]; then
    echo "   tt serve start Qwen3-0.6B --device-id 3 --port 8003 --detach"
  fi
fi

echo ""
echo "Detailed status:"
tt serve status 2>/dev/null || echo "  (tt serve status not available)"

exit 0
