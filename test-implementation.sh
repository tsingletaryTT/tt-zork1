#!/bin/bash
# test-implementation.sh - Verify simplified prompts and ASCII art integration
# Created: 2026-02-17

set -e

echo "========================================================================"
echo "  Implementation Verification Test"
echo "========================================================================"
echo ""

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Test 1: Verify prompt files exist and are simplified
echo "[Test 1] Verifying simplified persona prompts..."
echo ""

check_prompt() {
    local file=$1
    local max_lines=$2
    local lines=$(wc -l < "$file")

    if [ -f "$file" ]; then
        if [ "$lines" -le "$max_lines" ]; then
            echo -e "  ${GREEN}✓${NC} $file ($lines lines, ≤ $max_lines expected)"
        else
            echo -e "  ${RED}✗${NC} $file ($lines lines, > $max_lines max)"
            return 1
        fi
    else
        echo -e "  ${RED}✗${NC} $file not found"
        return 1
    fi
}

check_prompt "prompts/player/expert_speedrunner.txt" 60
check_prompt "prompts/player/naive_explorer.txt" 70
check_prompt "prompts/player/completionist.txt" 70
check_prompt "prompts/player/experimental.txt" 70

echo ""

# Test 2: Verify ASCII art model exists
echo "[Test 2] Verifying ASCII art specialist model..."
echo ""

if [ -d ~/models/Llama-3.2-1B-ASCII-merged ]; then
    model_size=$(du -sh ~/models/Llama-3.2-1B-ASCII-merged | cut -f1)
    echo -e "  ${GREEN}✓${NC} Model exists at ~/models/Llama-3.2-1B-ASCII-merged ($model_size)"

    # Check if model has all required files
    required_files=("config.json" "model.safetensors" "tokenizer.json")
    for file in "${required_files[@]}"; do
        if [ -f ~/models/Llama-3.2-1B-ASCII-merged/$file ]; then
            echo -e "  ${GREEN}✓${NC} Found $file"
        else
            echo -e "  ${RED}✗${NC} Missing $file"
        fi
    done
else
    echo -e "  ${RED}✗${NC} Model not found at ~/models/Llama-3.2-1B-ASCII-merged"
    echo "  Run: python scripts/merge_lora.py meta-llama/Llama-3.2-1B-Instruct AvaLovelace/LLaMA-ASCII-Art ~/models/Llama-3.2-1B-ASCII-merged"
fi

echo ""

# Test 3: Verify config updated
echo "[Test 3] Verifying config uses ASCII art model..."
echo ""

if grep -q "Llama-3.2-1B-ASCII-merged" config/llm_mode.yaml; then
    echo -e "  ${GREEN}✓${NC} Config updated to use ASCII art model"
else
    echo -e "  ${RED}✗${NC} Config still uses base model"
    echo "  Expected: model: Llama-3.2-1B-ASCII-merged"
fi

echo ""

# Test 4: Verify build succeeds
echo "[Test 4] Verifying build..."
echo ""

if [ -f zork-native ]; then
    echo -e "  ${GREEN}✓${NC} Binary exists: zork-native"
else
    echo -e "  ${YELLOW}⚠${NC} Binary not found, attempting build..."
    ./scripts/build_local.sh >/dev/null 2>&1
    if [ -f zork-native ]; then
        echo -e "  ${GREEN}✓${NC} Build successful"
    else
        echo -e "  ${RED}✗${NC} Build failed"
        exit 1
    fi
fi

echo ""

# Test 5: Verify code integration
echo "[Test 5] Verifying ASCII art integration in code..."
echo ""

if grep -q "scene_visualizer_generate" src/journey/monitor.c; then
    echo -e "  ${GREEN}✓${NC} monitor.c: scene_visualizer_generate() call found"
else
    echo -e "  ${RED}✗${NC} monitor.c: Missing scene_visualizer_generate() call"
fi

if grep -q "scene_visualizer_init" src/zmachine/frotz/src/dumb/dinit.c; then
    echo -e "  ${GREEN}✓${NC} dinit.c: scene_visualizer_init() call found"
else
    echo -e "  ${RED}✗${NC} dinit.c: Missing scene_visualizer_init() call"
fi

if grep -q "scene_visualizer_shutdown" src/zmachine/frotz/src/dumb/dinit.c; then
    echo -e "  ${GREEN}✓${NC} dinit.c: scene_visualizer_shutdown() call found"
else
    echo -e "  ${RED}✗${NC} dinit.c: Missing scene_visualizer_shutdown() call"
fi

echo ""

# Test 6: Verify no code fences in prompts
echo "[Test 6] Verifying prompts are clean (no code fences)..."
echo ""

for file in prompts/player/expert_speedrunner.txt prompts/player/naive_explorer.txt prompts/player/completionist.txt prompts/player/experimental.txt; do
    if grep -q '```' "$file"; then
        echo -e "  ${RED}✗${NC} $file: Contains code fences (\`\`\`)"
    else
        echo -e "  ${GREEN}✓${NC} $file: No code fences (clean)"
    fi
done

echo ""
echo "========================================================================"
echo "  Verification Complete"
echo "========================================================================"
echo ""
echo "Next steps:"
echo "  1. Ensure all 4 LLM servers are running:"
echo "     tt serve status"
echo ""
echo "  2. Test simplified prompts:"
echo "     ./zork-native game/zork1.z3"
echo "     > /mode enhanced"
echo "     > /play auto"
echo "     > /player naive"
echo ""
echo "  3. Test ASCII art (with specialist model):"
echo "     tt serve start ~/models/Llama-3.2-1B-ASCII-merged --device-id 1 --port 8001 --detach"
echo "     ./zork-native game/zork1.z3"
echo "     > /mode enhanced"
echo "     > look"
echo ""
