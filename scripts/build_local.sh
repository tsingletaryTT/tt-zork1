#!/usr/bin/env bash
#
# build_local.sh - Build Zork interpreter natively for local testing
#
# This script builds the Z-machine interpreter for the host architecture
# (x86_64 or ARM64) to enable rapid development and testing without hardware.
#
# Usage:
#   ./scripts/build_local.sh [clean|debug|release]
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
BUILD_DIR="build-native"
BINARY_NAME="zork-native"

# Compiler settings
CC="${CC:-cc}"
CFLAGS_COMMON="-std=c99 -Wall -Wextra -Wno-unused-parameter -DBUILD_NATIVE -DUSE_UTF8"
CFLAGS_DEBUG="-g -O0 -DDEBUG"
CFLAGS_RELEASE="-O3 -DNDEBUG"

echo -e "${GREEN}=== Building Zork Native Interpreter ===${NC}"
echo "Build type: $BUILD_TYPE"
echo "Build directory: $BUILD_DIR"
echo "Compiler: $CC"

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
    echo -e "${RED}Error: Frotz source not found in src/zmachine/frotz${NC}"
    echo "Please run: git clone --depth 1 https://gitlab.com/DavidGriffith/frotz.git src/zmachine/frotz"
    exit 1
fi

echo -e "${GREEN}Compiling source files...${NC}"

# Collect source files
FROTZ_COMMON_SRC="src/zmachine/frotz/src/common"
IO_SRC="src/io"
ZMACHINE_SRC="src/zmachine"

# Include directories
INCLUDES="-I${FROTZ_COMMON_SRC} -I${IO_SRC} -I${ZMACHINE_SRC}"

# Compile Frotz common core
echo "  Compiling Frotz Z-machine core..."
FROTZ_OBJS=""
for src in ${FROTZ_COMMON_SRC}/*.c; do
    # Prefix Frotz objects with frotz_ to avoid name collisions
    obj="$BUILD_DIR/frotz_$(basename ${src%.c}.o)"
    echo "    $(basename $src)"
    $CC $CFLAGS $INCLUDES -c "$src" -o "$obj"
    FROTZ_OBJS="$FROTZ_OBJS $obj"
done

# Compile Frotz dumb interface
echo "  Compiling Frotz dumb interface..."
DUMB_SRC="src/zmachine/frotz/src/dumb"
DUMB_OBJS=""
for src in ${DUMB_SRC}/*.c; do
    obj="$BUILD_DIR/dumb_$(basename ${src%.c}.o)"
    echo "    $(basename $src)"
    $CC $CFLAGS $INCLUDES -I${DUMB_SRC} -c "$src" -o "$obj"
    DUMB_OBJS="$DUMB_OBJS $obj"
done

# Compile blorb library if needed
BLORB_LIB="src/zmachine/frotz/src/blorb/blorblib.a"
if [ ! -f "$BLORB_LIB" ]; then
    echo "  Building blorb library..."
    $CC $CFLAGS -c "src/zmachine/frotz/src/blorb/blorblib.c" -o "src/zmachine/frotz/src/blorb/blorblib.o"
    ar rcs "$BLORB_LIB" "src/zmachine/frotz/src/blorb/blorblib.o"
fi

# Link everything together
echo "  Linking..."
$CC $CFLAGS \
    $DUMB_OBJS \
    $FROTZ_OBJS \
    $BLORB_LIB \
    -o "$BUILD_DIR/$BINARY_NAME"

# Create symlink in project root for convenience
ln -sf "$BUILD_DIR/$BINARY_NAME" "$BINARY_NAME"

echo -e "${GREEN}=== Build complete! ===${NC}"
echo ""
echo "Executable: $PROJECT_ROOT/$BINARY_NAME"
echo ""
echo "To run:"
echo "  ./$BINARY_NAME game/zork1.z3"
echo ""
echo "To clean:"
echo "  ./scripts/build_local.sh clean"
