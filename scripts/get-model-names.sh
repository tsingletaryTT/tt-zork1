#!/bin/bash
# Query actual model names from running servers

echo "Querying model names from servers..."
echo ""

# Query each port
for port in 8000 8001 8002 8003; do
  model=$(curl -s --max-time 2 http://localhost:$port/v1/models 2>/dev/null | jq -r '.data[0].id' 2>/dev/null)

  if [ -n "$model" ] && [ "$model" != "null" ]; then
    echo "Port $port: $model"
  else
    echo "Port $port: (not responding)"
  fi
done
