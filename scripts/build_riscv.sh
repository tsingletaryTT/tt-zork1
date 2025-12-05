#!/usr/bin/env bash
#
# build_riscv.sh - Cross-compile Zork interpreter for Tenstorrent RISC-V cores
#
# This script cross-compiles the Z-machine interpreter for RISC-V architecture
# to run on Tenstorrent Blackhole/Wormhole hardware.
#
# Prerequisites:
#   - RISC-V toolchain installed (riscv64-unknown-elf-gcc or similar)
#   - TT-Metal SDK installed and configured
#
# Usage:
#   ./scripts/build_riscv.sh [clean|debug|release]
#
# Arguments:
#   clean   - Clean build artifacts before building
#   debug   - Build with debug symbols (default)
#   release - Build with optimizations

set -e  # Exit on error
set -u  # Exit on undefined variable

# Color output for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

# Build configuration
BUILD_TYPE="${1:-debug}"
BUILD_DIR="build-riscv"
BINARY_NAME="zork-riscv"

# RISC-V toolchain settings
# These will need to be adjusted based on your specific toolchain
RISCV_TOOLCHAIN="${RISCV_TOOLCHAIN:-riscv64-unknown-elf}"
CC="${RISCV_TOOLCHAIN}-gcc"
OBJCOPY="${RISCV_TOOLCHAIN}-objcopy"

# Check for TT-Metal SDK
if [ -z "${TT_METAL_HOME:-}" ]; then
    echo -e "${YELLOW}Warning: TT_METAL_HOME not set${NC}"
    echo "Please set TT_METAL_HOME to your TT-Metal installation directory"
    echo "Example: export TT_METAL_HOME=/path/to/tt-metal"
    echo ""
    echo "Proceeding without TT-Metal SDK (standalone build)..."
    TT_METAL_INCLUDES=""
    TT_METAL_LIBS=""
else
    echo "TT-Metal SDK: $TT_METAL_HOME"
    TT_METAL_INCLUDES="-I$TT_METAL_HOME/tt_metal"
    TT_METAL_LIBS="-L$TT_METAL_HOME/build/lib"
fi

# Compiler settings
CFLAGS_COMMON="-std=c99 -Wall -Wextra -march=rv64imac -mabi=lp64 -DBUILD_TT_METAL $TT_METAL_INCLUDES"
CFLAGS_DEBUG="-g -O0 -DDEBUG"
CFLAGS_RELEASE="-O3 -DNDEBUG"

echo -e "${GREEN}=== Building Zork RISC-V Interpreter ===${NC}"
echo "Build type: $BUILD_TYPE"
echo "Build directory: $BUILD_DIR"
echo "Compiler: $CC"
echo "Target: RISC-V 64-bit"

# Check if compiler is available
if ! command -v "$CC" &> /dev/null; then
    echo -e "${RED}Error: RISC-V compiler '$CC' not found${NC}"
    echo ""
    echo "Please install a RISC-V toolchain:"
    echo "  - macOS: brew install riscv-tools"
    echo "  - Ubuntu: apt-get install gcc-riscv64-unknown-elf"
    echo ""
    echo "Or set RISCV_TOOLCHAIN environment variable to your toolchain prefix"
    echo "Example: export RISCV_TOOLCHAIN=riscv64-linux-gnu"
    exit 1
fi

# Clean if requested
if [ "$BUILD_TYPE" = "clean" ]; then
    echo -e "${YELLOW}Cleaning build artifacts...${NC}"
    rm -rf "$BUILD_DIR"
    rm -f "$BINARY_NAME"
    echo -e "${GREEN}Clean complete.${NC}"
    exit 0
fi

# Select build flags
if [ "$BUILD_TYPE" = "release" ]; then
    CFLAGS="$CFLAGS_COMMON $CFLAGS_RELEASE"
else
    CFLAGS="$CFLAGS_COMMON $CFLAGS_DEBUG"
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Check if Frotz source is available
if [ ! -d "src/zmachine/frotz" ]; then
    echo -e "${YELLOW}Warning: Frotz source not found in src/zmachine/frotz${NC}"
    echo -e "${YELLOW}Please run: git clone https://gitlab.com/DavidGriffith/frotz.git src/zmachine/frotz${NC}"
    echo ""
    echo "Cannot proceed with RISC-V build without source code."
    exit 1
fi

# Compile source files
echo -e "${GREEN}Compiling source files for RISC-V...${NC}"

# This will be expanded once Frotz integration is complete
# For now, just show what would be compiled
echo "Source files to compile:"
echo "  - src/zmachine/main.c (or Frotz core)"
echo "  - src/io/io_ttmetal.c (TT-Metal host interface)"
echo "  - src/parser/parser.c"

echo ""
echo -e "${YELLOW}RISC-V build not yet fully implemented${NC}"
echo "Waiting for Frotz integration (Phase 1.2)"
echo ""
echo "Next steps:"
echo "  1. Clone Frotz: git clone https://gitlab.com/DavidGriffith/frotz.git src/zmachine/frotz"
echo "  2. Create TT-Metal I/O layer: src/io/io_ttmetal.c"
echo "  3. Update this script to compile full source tree"

exit 0
