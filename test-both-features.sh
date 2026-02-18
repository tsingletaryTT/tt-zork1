#!/bin/bash
# test-both-features.sh - Test simplified prompts and ASCII art
# Created: 2026-02-17

set -e

echo "========================================================================"
echo "  Testing Simplified Prompts + ASCII Art"
echo "========================================================================"
echo ""

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}[Check 1]${NC} Verifying all 4 LLM servers are running..."
echo ""

required_ports=(8000 8001 8002 8003)
for port in "${required_ports[@]}"; do
    if tt serve status 2>&1 | grep -q "Port: $port"; then
        echo -e "  ${GREEN}✓${NC} Server running on port $port"
    else
        echo -e "  ${RED}✗${NC} No server on port $port"
        echo "  Run: ./scripts/start-four-llms.sh"
        exit 1
    fi
done

echo ""
echo -e "${BLUE}[Check 2]${NC} Verifying ASCII art model is running..."
echo ""

if tt serve status 2>&1 | grep -A 2 "Port: 8001" | grep -q "Llama-3.2-1B-ASCII-merged"; then
    echo -e "  ${GREEN}✓${NC} ASCII art specialist model running on port 8001"
else
    echo -e "  ${YELLOW}⚠${NC} Port 8001 not using ASCII art model"
    echo "  Current model:"
    tt serve status 2>&1 | grep -B 1 "Port: 8001" | head -1
    echo ""
    echo "  To fix:"
    echo "    tt stop Llama-3.2-1B-Instruct --device-id 1"
    echo "    tt serve start ~/models/Llama-3.2-1B-ASCII-merged --device-id 1 --port 8001 --detach"
fi

echo ""
echo "========================================================================"
echo "  How to Use the New Features"
echo "========================================================================"
echo ""

echo -e "${YELLOW}Feature 1: Simplified Prompts (Clean AI Commands)${NC}"
echo ""
echo "  WRONG WAY (uses old verbose prompt):"
echo -e "    ${RED}> /play auto${NC}  ← This defaults to OLD strategy!"
echo ""
echo "  CORRECT WAY (uses new simplified prompt):"
echo -e "    ${GREEN}> /player naive${NC}      ← Set persona FIRST"
echo -e "    ${GREEN}> /play auto${NC}         ← Then enable auto-play"
echo ""
echo "  Available personas:"
echo "    /player naive          - First-time player, learns naturally"
echo "    /player expert         - Speedrunner, knows everything"
echo "    /player completionist  - 100% hunter, methodical"
echo "    /player experimental   - Boundary tester, creative"
echo ""
echo "  Stop auto-play:"
echo "    > /play solo"
echo ""

echo -e "${YELLOW}Feature 2: ASCII Art (Appears on Location Changes)${NC}"
echo ""
echo "  The art appears automatically when you move to new locations!"
echo ""
echo "  Try this:"
echo -e "    ${GREEN}> /mode enhanced${NC}  ← Enable LLM features"
echo -e "    ${GREEN}> look${NC}            ← See current location art"
echo -e "    ${GREEN}> go north${NC}        ← Move to new location → art appears!"
echo ""

echo "========================================================================"
echo "  Quick Test Commands"
echo "========================================================================"
echo ""

echo "Start the game:"
echo "  cd ~/code/tt-zork1"
echo "  ./zork-native game/zork1.z3"
echo ""

echo "Test 1: Simplified Prompts (AI plays with clean commands)"
echo -e "  ${GREEN}> /mode enhanced${NC}"
echo -e "  ${GREEN}> /player naive${NC}      ← Use simplified naive prompt"
echo -e "  ${GREEN}> /play auto${NC}         ← Watch AI play"
echo "  Expected: Clean commands like 'examine mailbox', 'go north'"
echo "  NOT: '**Strategy: EXPLORE**' markdown text"
echo ""

echo "Test 2: ASCII Art (Appears on location changes)"
echo -e "  ${GREEN}> /play solo${NC}         ← Stop auto-play"
echo -e "  ${GREEN}> /mode enhanced${NC}     ← Enable LLM features"
echo -e "  ${GREEN}> look${NC}               ← See ASCII art for current location"
echo -e "  ${GREEN}> open window${NC}        ← Move inside"
echo "  Expected: Beautiful ASCII art box appears"
echo ""

echo "Test 3: Both Together (AI plays with ASCII art)"
echo -e "  ${GREEN}> /mode enhanced${NC}"
echo -e "  ${GREEN}> /player naive${NC}"
echo -e "  ${GREEN}> /play auto${NC}"
echo "  Expected: AI generates commands + ASCII art on each move"
echo ""

echo "How to quit:"
echo "  > quit                ← Without slash!"
echo ""

echo "Get help:"
echo "  > /help               ← Show all slash commands"
echo "  > /status             ← Show current settings"
echo ""

echo "========================================================================"
echo "  Troubleshooting"
echo "========================================================================"
echo ""

echo "Problem: AI outputs markdown like '**Strategy: EXPLORE**'"
echo "Solution: You forgot to set persona first!"
echo -e "  ${GREEN}> /player naive${NC}  ← Do this BEFORE /play auto"
echo ""

echo "Problem: No ASCII art appears"
echo "Solution: Check artist server is using merged model"
echo "  tt serve status | grep -A 2 '8001'"
echo "  Should show: Llama-3.2-1B-ASCII-merged"
echo ""

echo "Problem: '/quit' doesn't work"
echo "Solution: Use 'quit' without the slash"
echo -e "  ${GREEN}> quit${NC}  ← Correct (this is a game command)"
echo "  > /quit  ← Wrong (slash commands are our custom commands)"
echo ""

echo "========================================================================"
echo "  Ready to Test!"
echo "========================================================================"
echo ""
echo "Start the game now:"
echo "  ./zork-native game/zork1.z3"
echo ""
