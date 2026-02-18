#!/bin/bash
# A/B Comparison Script - Multi-Agent vs Single 8B Model
# Automates the testing process and collects metrics

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
RESULTS_DIR="$PROJECT_DIR/test_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

echo "╔════════════════════════════════════════════════════════════╗"
echo "║  A/B TEST: Multi-Agent (4× 1B) vs Single Model (1× 8B)   ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Create results directory
mkdir -p "$RESULTS_DIR"

echo "This script will help you compare:"
echo "  • Setup A: 4× Llama-3.2-1B (specialized, parallel)"
echo "  • Setup B: 1× Llama-3.1-8B (unified, serial)"
echo ""
echo "You'll play two ~10 minute sessions and we'll collect metrics."
echo ""

read -p "Continue? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted."
    exit 0
fi

# =============================================================================
# TEST 1: Multi-Agent Mode (Current Setup)
# =============================================================================

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TEST 1: Multi-Agent Mode (4× 1B Models)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "Starting 4× 1B models..."
"$SCRIPT_DIR/start-four-llms.sh" --no-wait

echo ""
echo "Waiting for all 4 servers to be healthy..."
sleep 60

# Health check
HEALTHY=0
for port in 8000 8001 8002 8003; do
    if curl -s --max-time 2 http://localhost:$port/health > /dev/null 2>&1; then
        echo "  ✅ Port $port healthy"
        HEALTHY=$((HEALTHY + 1))
    else
        echo "  ❌ Port $port not responding"
    fi
done

if [ $HEALTHY -lt 4 ]; then
    echo ""
    echo "⚠️  Not all servers healthy. Continue anyway? (y/N)"
    read -p "> " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted. Check server status with: tt serve status"
        exit 1
    fi
fi

echo ""
echo "🎮 Now play Zork for ~10 minutes with the multi-agent setup."
echo ""
echo "Try these test scenarios:"
echo "  1. Complex commands: 'I want to carefully open the small mailbox'"
echo "  2. Pronoun resolution: 'take it', 'open that'"
echo "  3. Visit 5-10 locations (test ASCII art variety)"
echo "  4. Try /play auto for 5 moves (test player agent)"
echo ""
echo "Press ENTER when ready to start..."
read

# Start game with logging
LOG_FILE="$RESULTS_DIR/test1_multi_agent_${TIMESTAMP}.log"
echo "Logging to: $LOG_FILE"
echo "Test started at: $(date)" > "$LOG_FILE"
echo "" >> "$LOG_FILE"

# Run game (output goes to both terminal and log)
export ZORK_LLM_CONFIG="$PROJECT_DIR/config/llm_mode.yaml"
"$PROJECT_DIR/play-zork-with-llm.sh" 2>&1 | tee -a "$LOG_FILE"

echo ""
echo "Test 1 complete! Results saved to: $LOG_FILE"
echo ""

# Stop servers
echo "Stopping multi-agent servers..."
"$SCRIPT_DIR/stop-four-llms.sh"
sleep 5

# =============================================================================
# TEST 2: Single 8B Model
# =============================================================================

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TEST 2: Single 8B Model (Tensor Parallelism)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "Starting 8B model with tensor parallelism..."
"$SCRIPT_DIR/start-single-8b.sh"

echo ""
echo "🎮 Now play Zork for ~10 minutes with the 8B model."
echo ""
echo "Try the SAME test scenarios:"
echo "  1. Complex commands: 'I want to carefully open the small mailbox'"
echo "  2. Pronoun resolution: 'take it', 'open that'"
echo "  3. Visit 5-10 locations (test ASCII art creativity)"
echo "  4. Try /play auto for 5 moves (test strategic depth)"
echo ""
echo "Compare to Test 1:"
echo "  • Does translation feel smarter?"
echo "  • Is ASCII art more creative/compositional?"
echo "  • Does it feel slower? (measure subjectively)"
echo ""
echo "Press ENTER when ready to start..."
read

# Start game with logging
LOG_FILE="$RESULTS_DIR/test2_single_8b_${TIMESTAMP}.log"
echo "Logging to: $LOG_FILE"
echo "Test started at: $(date)" > "$LOG_FILE"
echo "" >> "$LOG_FILE"

# Run game
export ZORK_LLM_CONFIG="$PROJECT_DIR/config/llm_mode_8b.yaml"
"$PROJECT_DIR/play-zork-with-llm.sh" 2>&1 | tee -a "$LOG_FILE"

echo ""
echo "Test 2 complete! Results saved to: $LOG_FILE"
echo ""

# Stop server
echo "Stopping 8B model..."
"$SCRIPT_DIR/stop-single-8b.sh"

# =============================================================================
# ANALYSIS
# =============================================================================

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "A/B TEST COMPLETE!"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "Test results saved to:"
echo "  Test 1 (Multi-Agent): $RESULTS_DIR/test1_multi_agent_${TIMESTAMP}.log"
echo "  Test 2 (Single 8B):   $RESULTS_DIR/test2_single_8b_${TIMESTAMP}.log"
echo ""

echo "Next steps:"
echo "  1. Review logs for quality differences"
echo "  2. Run: ./scripts/analyze-results.sh $TIMESTAMP"
echo "  3. Document decision in docs/MODEL_COMPARISON.md"
echo ""

# Create summary file
SUMMARY_FILE="$RESULTS_DIR/summary_${TIMESTAMP}.md"
cat > "$SUMMARY_FILE" << EOF
# A/B Test Summary - $TIMESTAMP

## Configuration

**Test 1: Multi-Agent**
- Models: 4× Llama-3.2-1B-Instruct
- Deployment: One per chip (devices 0-3)
- Ports: 8000-8003
- Execution: Parallel (4 tasks simultaneously)

**Test 2: Single 8B**
- Model: 1× Llama-3.1-8B-Instruct
- Deployment: Tensor parallel across 4 chips
- Port: 9000
- Execution: Serial (4 tasks queued)

## Metrics to Evaluate

### Quantitative
- [ ] Latency: Time from command to response
- [ ] Accuracy: Translation success rate
- [ ] Memory: VRAM usage per setup

### Qualitative
- [ ] Translation: Handles ambiguity and pronouns?
- [ ] ASCII Art: Template-based vs creative composition?
- [ ] Player Agent: Strategic depth and adaptability?
- [ ] Overall Experience: Which feels more magical?

## Test Scenarios Used

1. Complex translation: "I want to carefully open the small mailbox"
2. Pronoun resolution: "take it", "open that"
3. Location variety: 5-10 different rooms
4. Auto-play mode: 5 strategic moves

## Results

### Test 1 (Multi-Agent 4× 1B)

**Translation Quality:**
- Notes:

**ASCII Art:**
- Notes:

**Player Intelligence:**
- Notes:

**Latency:**
- Notes:

### Test 2 (Single 8B)

**Translation Quality:**
- Notes:

**ASCII Art:**
- Notes:

**Player Intelligence:**
- Notes:

**Latency:**
- Notes:

## Recommendation

### Winner: [TBD]

**Reasoning:**

**Action Items:**
- [ ] Update default config
- [ ] Document decision in CLAUDE.md
- [ ] Archive losing setup or keep both as options?

EOF

echo "Created summary template: $SUMMARY_FILE"
echo "Please fill in your observations!"
echo ""
