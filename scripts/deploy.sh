#!/usr/bin/env bash
#
# deploy.sh - Deploy Zork to Tenstorrent hardware environment
#
# This script is designed to run on the target hardware environment
# (Ubuntu 22.04 with Wormhole/Blackhole cards) after git pull.
#
# Usage:
#   ./scripts/deploy.sh [wormhole|blackhole]
#
# Prerequisites (on target system):
#   - TT-Metal SDK installed
#   - RISC-V toolchain available
#   - Tenstorrent hardware detected

set -e
set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

HARDWARE="${1:-}"

echo -e "${GREEN}=== Deploying Zork to Tenstorrent Hardware ===${NC}"

# Detect or validate hardware
if [ -z "$HARDWARE" ]; then
    echo "Detecting Tenstorrent hardware..."

    # Use tt-smi to detect hardware (if available)
    if command -v tt-smi &> /dev/null; then
        DETECTED=$(tt-smi -q | grep -o "Wormhole\|Blackhole" | head -1 || echo "unknown")
        if [ "$DETECTED" != "unknown" ]; then
            HARDWARE=$(echo "$DETECTED" | tr '[:upper:]' '[:lower:]')
            echo -e "${GREEN}Detected: $HARDWARE${NC}"
        fi
    fi

    if [ -z "$HARDWARE" ]; then
        echo -e "${YELLOW}Could not auto-detect hardware${NC}"
        echo "Usage: $0 [wormhole|blackhole]"
        exit 1
    fi
fi

echo "Target hardware: $HARDWARE"

# Verify TT-Metal installation
if [ -z "${TT_METAL_HOME:-}" ]; then
    echo -e "${RED}Error: TT_METAL_HOME not set${NC}"
    echo "Please ensure TT-Metal SDK is installed and configured"
    echo "See: https://github.com/tenstorrent/tt-metal/blob/main/INSTALLING.md"
    exit 1
fi

echo "TT-Metal SDK: $TT_METAL_HOME"

# Build for RISC-V
echo ""
echo -e "${GREEN}Building for RISC-V target...${NC}"
./scripts/build_riscv.sh release

if [ ! -f "build-riscv/zork-riscv" ]; then
    echo -e "${RED}Build failed - binary not found${NC}"
    exit 1
fi

# Deploy to hardware
echo ""
echo -e "${GREEN}Deploying to $HARDWARE...${NC}"

# This will be implemented once we understand TT-Metal's deployment model
# For now, just show what would happen
echo "Deployment steps:"
echo "  1. Load RISC-V binary to device memory"
echo "  2. Configure host interface for I/O"
echo "  3. Launch interpreter on RISC-V core"
echo "  4. Set up communication channels"

echo ""
echo -e "${YELLOW}Deployment procedure not yet implemented${NC}"
echo "Waiting for TT-Metal integration (Phase 1.3)"

# Run basic hardware tests
echo ""
echo "Verifying hardware status..."
if command -v tt-smi &> /dev/null; then
    tt-smi -q
else
    echo -e "${YELLOW}tt-smi not available${NC}"
fi

echo ""
echo -e "${GREEN}Deployment preparation complete${NC}"
echo ""
echo "Next steps:"
echo "  1. Complete RISC-V build integration"
echo "  2. Implement TT-Metal host interface"
echo "  3. Test binary on hardware"
