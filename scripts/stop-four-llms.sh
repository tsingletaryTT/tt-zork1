#!/bin/bash
# Stop all 4 LLM servers

echo "🛑 Stopping 4-LLM Architecture..."
echo ""

# Stop each server by device ID
echo "Stopping Command Translator (Device 0)..."
tt stop Qwen3-0.6B --device-id 0 2>/dev/null || echo "  (not running)"

echo "Stopping ASCII Artist (Device 1)..."
tt stop Qwen3-0.6B --device-id 1 2>/dev/null || echo "  (not running)"

echo "Stopping Dungeon Master (Device 2)..."
tt stop Qwen3-0.6B --device-id 2 2>/dev/null || echo "  (not running)"

echo "Stopping AI Player (Device 3)..."
tt stop Qwen3-0.6B --device-id 3 2>/dev/null || echo "  (not running)"

echo ""
echo "✅ All servers stopped"
echo ""

# Show remaining servers (should be empty)
tt serve status
