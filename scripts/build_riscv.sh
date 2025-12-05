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

# RISC-V toolchain configuration
# Try multiple common toolchain prefixes
RISCV_PREFIXES=(
    "riscv64-unknown-elf"
    "riscv64-linux-gnu"
    "riscv64-unknown-linux-gnu"
)

# Find available RISC-V toolchain that actually works
RISCV_PREFIX=""
for prefix in "${RISCV_PREFIXES[@]}"; do
    if command -v "${prefix}-gcc" &> /dev/null; then
        # Test if the toolchain can compile with standard headers AND LP64 ABI
        if echo '#include <string.h>
int main() { return 0; }' | "${prefix}-gcc" -march=rv64imac -mabi=lp64 -x c -c - -o /dev/null 2>/dev/null; then
            RISCV_PREFIX="$prefix"
            break
        fi
    fi
done

# Allow override via environment variable
if [ -n "${RISCV_TOOLCHAIN:-}" ]; then
    RISCV_PREFIX="$RISCV_TOOLCHAIN"
fi

CC="${RISCV_PREFIX}-gcc"
AR="${RISCV_PREFIX}-ar"
RANLIB="${RISCV_PREFIX}-ranlib"
OBJCOPY="${RISCV_PREFIX}-objcopy"

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

# RISC-V architecture flags
# Targeting Tenstorrent Blackhole: RV64IMAC
# - RV64: 64-bit base ISA
# - I: Integer instructions
# - M: Multiply/divide
# - A: Atomic instructions
# - C: Compressed instructions (16-bit)
ARCH_FLAGS="-march=rv64imac -mabi=lp64"

# Compiler settings
CFLAGS_COMMON="-std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -Wno-unused-parameter"
CFLAGS_COMMON="$CFLAGS_COMMON $ARCH_FLAGS"
CFLAGS_COMMON="$CFLAGS_COMMON -DBUILD_RISCV -DUSE_UTF8"
CFLAGS_COMMON="$CFLAGS_COMMON -static $TT_METAL_INCLUDES"  # Static linking for bare-metal
CFLAGS_DEBUG="-g -O0 -DDEBUG"
CFLAGS_RELEASE="-O3 -DNDEBUG"

echo -e "${GREEN}=== Building Zork RISC-V Interpreter ===${NC}"
echo "Build type: $BUILD_TYPE"
echo "Build directory: $BUILD_DIR"
echo "Architecture: rv64imac (Tenstorrent Blackhole compatible)"

# Check if compiler is available
if [ -z "$RISCV_PREFIX" ] || ! command -v "$CC" &> /dev/null; then
    echo -e "${RED}ERROR: No working RISC-V toolchain found!${NC}"
    echo ""
    echo "A RISC-V compiler was found, but it's missing standard library headers."
    echo "This usually means you have a bare-metal toolchain (riscv64-unknown-elf-gcc)"
    echo "without newlib/glibc installed."
    echo ""
    echo "Recommended solution - install the Linux-targeted toolchain with libc:"
    echo ""
    echo "Ubuntu/Debian:"
    echo "   sudo apt-get install gcc-riscv64-linux-gnu g++-riscv64-linux-gnu libc6-dev-riscv64-cross"
    echo ""
    echo "macOS (via Homebrew):"
    echo "   brew tap riscv-software-src/riscv"
    echo "   brew install riscv-gnu-toolchain"
    echo ""
    echo "Or build from source with newlib:"
    echo "   git clone https://github.com/riscv/riscv-gnu-toolchain"
    echo "   cd riscv-gnu-toolchain"
    echo "   ./configure --prefix=/opt/riscv --with-arch=rv64imac --with-abi=lp64"
    echo "   make"
    echo ""
    echo "Or manually specify a working toolchain:"
    echo "   export RISCV_TOOLCHAIN=riscv64-linux-gnu"
    echo "   ./scripts/build_riscv.sh"
    echo ""
    exit 1
fi

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

# Collect source directories
FROTZ_COMMON_SRC="src/zmachine/frotz/src/common"
DUMB_SRC="src/zmachine/frotz/src/dumb"

# Include directories
INCLUDES="-I${FROTZ_COMMON_SRC} -I${DUMB_SRC}"

# Compile Frotz common core
echo "  Compiling Frotz Z-machine core..."
FROTZ_OBJS=""
for src in ${FROTZ_COMMON_SRC}/*.c; do
    obj="$BUILD_DIR/frotz_$(basename ${src%.c}.o)"
    echo "    $(basename $src)"
    $CC $CFLAGS $INCLUDES -c "$src" -o "$obj"
    FROTZ_OBJS="$FROTZ_OBJS $obj"
done

# Compile Frotz dumb interface
echo "  Compiling Frotz dumb interface..."
DUMB_OBJS=""
for src in ${DUMB_SRC}/*.c; do
    obj="$BUILD_DIR/dumb_$(basename ${src%.c}.o)"
    echo "    $(basename $src)"
    $CC $CFLAGS $INCLUDES -I${DUMB_SRC} -c "$src" -o "$obj"
    DUMB_OBJS="$DUMB_OBJS $obj"
done

# Compile blorb library
echo "  Building blorb library..."
$CC $CFLAGS $INCLUDES -c "src/zmachine/frotz/src/blorb/blorblib.c" -o "$BUILD_DIR/blorblib.o"
BLORB_LIB="$BUILD_DIR/blorblib.a"
$AR rcs "$BLORB_LIB" "$BUILD_DIR/blorblib.o"
$RANLIB "$BLORB_LIB"

# Link everything together
echo "  Linking..."
$CC $CFLAGS \
    $DUMB_OBJS \
    $FROTZ_OBJS \
    $BLORB_LIB \
    -o "$BUILD_DIR/$BINARY_NAME"

# Create symlink in project root for convenience
ln -sf "$BUILD_DIR/$BINARY_NAME" "$BINARY_NAME"

# Display binary information
echo ""
echo -e "${GREEN}=== Build complete! ===${NC}"
echo ""
echo "Executable: $PROJECT_ROOT/$BINARY_NAME"

# Show binary info
if command -v file &> /dev/null; then
    echo ""
    echo "Binary info:"
    file "$BUILD_DIR/$BINARY_NAME"
fi

if command -v ${RISCV_PREFIX}-size &> /dev/null; then
    echo ""
    echo "Binary size:"
    ${RISCV_PREFIX}-size "$BUILD_DIR/$BINARY_NAME"
fi

if command -v ${RISCV_PREFIX}-readelf &> /dev/null; then
    echo ""
    echo "ELF header:"
    ${RISCV_PREFIX}-readelf -h "$BUILD_DIR/$BINARY_NAME" | grep -E "(Machine|Class|Data|Entry)"
fi

echo ""
echo "To test on QEMU:"
echo "  qemu-riscv64 -L /usr/riscv64-linux-gnu ./$BINARY_NAME game/zork1.z3"
echo ""
echo "To deploy to Tenstorrent:"
echo "  # Copy to hardware environment with Wormhole/Blackhole card"
echo "  # Use TT-Metal tools to load and execute on RISC-V cores"
echo ""
echo "To clean:"
echo "  ./scripts/build_riscv.sh clean"
