#!/bin/bash
# Switch between LLM architecture modes

set -e

CONFIG_DIR="config"
CONFIG_FILE="$CONFIG_DIR/llm_mode.yaml"
EXAMPLE_FILE="$CONFIG_DIR/llm_mode.yaml.example"

echo "🔄 LLM Architecture Mode Switcher"
echo ""

# Show current mode if config exists
if [ -f "$CONFIG_FILE" ]; then
  CURRENT=$(grep "^mode:" "$CONFIG_FILE" | awk '{print $2}')
  echo "Current mode: $CURRENT"
  echo ""
fi

# Show available presets
echo "Available presets:"
echo "  1. multi_agent     - 4× Qwen3-0.6B (one per chip)"
echo "  2. unified         - 1× Qwen3-32B (tensor parallel)"
echo "  3. hybrid_image    - 2 chips Qwen3-8B + 2 chips SDXL"
echo "  4. minimal         - 1× Qwen3-0.6B on single chip"
echo "  5. research        - Mixed models per task"
echo "  6. custom          - Edit config manually"
echo ""

if [ -z "$1" ]; then
  read -p "Select mode [1-6]: " choice
else
  choice="$1"
fi

case $choice in
  1|multi_agent)
    MODE="multi_agent"
    echo "Switching to: Multi-Agent (4× Qwen3-0.6B)"
    ;;
  2|unified)
    MODE="unified"
    echo "Switching to: Unified (1× Qwen3-32B)"
    ;;
  3|hybrid_image)
    MODE="hybrid_image"
    echo "Switching to: Hybrid (Text + Image)"
    ;;
  4|minimal)
    MODE="minimal"
    echo "Switching to: Minimal (1 chip only)"
    ;;
  5|research)
    MODE="research"
    echo "Switching to: Research (mixed models)"
    ;;
  6|custom)
    echo "Opening config for manual edit..."
    ${EDITOR:-vim} "$CONFIG_FILE"
    exit 0
    ;;
  *)
    echo "Invalid choice: $choice"
    exit 1
    ;;
esac

# Create config directory if needed
mkdir -p "$CONFIG_DIR"

# Extract the selected preset from example file
echo "Extracting preset from $EXAMPLE_FILE..."

# Find the preset section
awk "
  /^# ===.*PRESET/ { in_preset=0 }
  /^mode: $MODE/ { in_preset=1 }
  in_preset && /^mode:/ { print; next }
  in_preset && /^#mode:/ { next }
  in_preset && /^endpoints:/ { in_endpoints=1 }
  in_preset && in_endpoints && /^#  / { sub(/^#/, \"\"); print; next }
  in_preset && in_endpoints && /^  [a-z]/ { print; next }
  in_preset && in_endpoints && /^$/ { next }
  in_preset && in_endpoints && /^# ==/ { exit }
" "$EXAMPLE_FILE" > "$CONFIG_FILE.tmp"

# If extraction succeeded, use it
if [ -s "$CONFIG_FILE.tmp" ]; then
  mv "$CONFIG_FILE.tmp" "$CONFIG_FILE"
  echo "✅ Configuration updated!"
else
  # Fallback: copy entire example and uncomment the section
  echo "⚠️  Extraction failed, using fallback method..."

  # Just set the mode line and include relevant section
  cat > "$CONFIG_FILE" << EOF
# Auto-generated configuration
# Mode: $MODE
# Generated: $(date)

mode: $MODE

# See config/llm_mode.yaml.example for full configuration options
# and other presets.

# TODO: Configure endpoints for $MODE mode
# Copy from $EXAMPLE_FILE preset section
EOF

  echo "⚠️  Please manually configure endpoints in: $CONFIG_FILE"
  echo "    Reference: $EXAMPLE_FILE"
fi

echo ""
echo "Configuration file: $CONFIG_FILE"
echo ""
echo "Next steps:"
echo "  1. Review config: cat $CONFIG_FILE"
echo "  2. Stop current servers: ./scripts/stop-four-llms.sh"
echo "  3. Start new mode: ./scripts/start-llms.sh"
echo ""
