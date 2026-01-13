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
CFLAGS_COMMON="-std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -Wno-unused-parameter -DBUILD_NATIVE -DUSE_UTF8"
CFLAGS_DEBUG="-g -O0 -DDEBUG"
CFLAGS_RELEASE="-O3 -DNDEBUG"

# Check for libcurl (needed for LLM features)
if command -v pkg-config &> /dev/null && pkg-config --exists libcurl; then
    echo "libcurl: Found"
    CURL_CFLAGS=$(pkg-config --cflags libcurl)
    CURL_LIBS=$(pkg-config --libs libcurl)
else
    echo -e "${YELLOW}Warning: libcurl not found. LLM features will be disabled.${NC}"
    echo "To install libcurl:"
    echo "  macOS:   brew install curl"
    echo "  Ubuntu:  sudo apt-get install libcurl4-openssl-dev"
    echo "  Fedora:  sudo dnf install libcurl-devel"
    echo ""
    CURL_CFLAGS=""
    CURL_LIBS=""
fi

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
    CFLAGS="$CFLAGS_COMMON $CFLAGS_RELEASE $CURL_CFLAGS"
else
    CFLAGS="$CFLAGS_COMMON $CFLAGS_DEBUG $CURL_CFLAGS"
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Check if Frotz source is available
if [ ! -d "src/zmachine/frotz" ]; then
    echo -e "${RED}Error: Frotz source not found in src/zmachine/frotz${NC}"
    echo "Please run: git clone --depth 1 https://gitlab.com/DavidGriffith/frotz.git src/zmachine/frotz"
    exit 1
fi

# Generate Frotz header files if they don't exist or are incomplete
NEEDS_REGEN=0
if [ ! -f "src/zmachine/frotz/src/common/defs.h" ] || [ ! -f "src/zmachine/frotz/src/common/hash.h" ]; then
    NEEDS_REGEN=1
elif ! grep -q "GIT_HASH" src/zmachine/frotz/src/common/defs.h 2>/dev/null; then
    NEEDS_REGEN=1
fi

if [ $NEEDS_REGEN -eq 1 ]; then
    echo -e "${GREEN}Generating Frotz header files...${NC}"
    cd src/zmachine/frotz
    make defs.h hash.h 2>/dev/null || {
        # If make fails, manually generate the files
        mkdir -p src/common
        echo "  Generating defs.h..."
        cat > src/common/defs.h << 'EOF'
#ifndef COMMON_DEFINES_H
#define COMMON_DEFINES_H
#define VERSION "2.56pre"
#define GIT_HASH "unknown"
#define GIT_DATE "unknown"
#define RELEASE_NOTES "Development release"
#define UNIX
#define MAX_UNDO_SLOTS 500
#define MAX_FILE_NAME 80
#define TEXT_BUFFER_SIZE 512
#define INPUT_BUFFER_SIZE 200
#define STACK_SIZE 1024
#define USE_UTF8
#endif /* COMMON_DEFINES_H */
EOF
        echo "  Generating hash.h..."
        cat > src/common/hash.h << 'EOF'
#ifndef COMMON_HASH_H
#define COMMON_HASH_H
#endif /* COMMON_HASH_H */
EOF
    }
    cd "$PROJECT_ROOT"
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

# Compile LLM subsystem (if libcurl available)
if [ -n "$CURL_LIBS" ]; then
    echo "  Compiling LLM subsystem..."
    LLM_SRC="src/llm"
    LLM_OBJS=""
    for src in ${LLM_SRC}/*.c; do
        obj="$BUILD_DIR/llm_$(basename ${src%.c}.o)"
        echo "    $(basename $src)"
        $CC $CFLAGS $INCLUDES -I${LLM_SRC} -c "$src" -o "$obj"
        LLM_OBJS="$LLM_OBJS $obj"
    done
else
    LLM_OBJS=""
fi

# Compile blorb library
echo "  Building blorb library..."
$CC $CFLAGS $INCLUDES -c "src/zmachine/frotz/src/blorb/blorblib.c" -o "$BUILD_DIR/blorblib.o"
BLORB_LIB="$BUILD_DIR/blorblib.a"
ar rcs "$BLORB_LIB" "$BUILD_DIR/blorblib.o"
ranlib "$BLORB_LIB"

# Link everything together
echo "  Linking..."
$CC $CFLAGS \
    $DUMB_OBJS \
    $FROTZ_OBJS \
    $LLM_OBJS \
    $BLORB_LIB \
    $CURL_LIBS \
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
