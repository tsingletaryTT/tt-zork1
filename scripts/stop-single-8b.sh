#!/bin/bash
# Stop Single 8B Model

set -e

echo "🛑 Stopping single 8B model..."

# Stop all servers (since we're using tensor parallelism across all devices)
tt serve stop --all

echo "✅ Stopped"
echo ""
echo "To restart: ./scripts/start-single-8b.sh"
