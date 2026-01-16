#!/usr/bin/env bash
#
# build_blackhole_kernel.sh - Build Zork kernel for Blackhole RISC-V cores
#
# This script compiles the Zork Z-machine interpreter as a TT-Metal kernel
# for execution on Tenstorrent Blackhole RISC-V cores.
#
# PROOF OF CONCEPT - Quick and dirty, we'll clean it up after it works!
#

set -e  # Exit on error

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  Building Zork Kernel for Blackhole RISC-V             ║${NC}"
echo -e "${GREEN}║  Historic First: 1977 Game → 2026 AI Accelerator       ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════════════╝${NC}"
echo

# Project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

# Build directory
BUILD_DIR="build-blackhole-kernel"
mkdir -p "$BUILD_DIR"

# TT-Metal paths
TT_METAL_HOME="${TT_METAL_HOME:-/home/ttuser/tt-metal}"
if [ ! -d "$TT_METAL_HOME" ]; then
    echo -e "${RED}ERROR: TT_METAL_HOME not found: $TT_METAL_HOME${NC}"
    exit 1
fi

# RISC-V toolchain
RISCV_CC="/opt/tenstorrent/sfpi/compiler/bin/riscv-tt-elf-gcc"
RISCV_AR="/opt/tenstorrent/sfpi/compiler/bin/riscv-tt-elf-ar"

if [ ! -f "$RISCV_CC" ]; then
    echo -e "${RED}ERROR: RISC-V compiler not found: $RISCV_CC${NC}"
    exit 1
fi

echo "Toolchain: $RISCV_CC"
echo "TT-Metal: $TT_METAL_HOME"
echo "Build dir: $BUILD_DIR"
echo

# Compiler flags
CFLAGS="-std=c99 -O3 -static"
CFLAGS="$CFLAGS -D_DEFAULT_SOURCE"  # Enable POSIX extensions (strdup, strndup)
CFLAGS="$CFLAGS -DBUILD_BLACKHOLE"  # Enable Blackhole mode
CFLAGS="$CFLAGS -DNO_BASENAME -DNO_STRRCHR"  # Use Frotz compat functions
CFLAGS="$CFLAGS -include string.h"  # Force include for newlib
CFLAGS="$CFLAGS -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-int-conversion"

# Include paths
INCLUDES="-I$PROJECT_ROOT/src/zmachine"
INCLUDES="$INCLUDES -I$PROJECT_ROOT/src/zmachine/frotz/src/common"
INCLUDES="$INCLUDES -I$PROJECT_ROOT/src/zmachine/frotz/src/dumb"
INCLUDES="$INCLUDES -I$TT_METAL_HOME/tt_metal/hw/inc"
INCLUDES="$INCLUDES -I$TT_METAL_HOME/build/libexec/tt-metalium/tt_metal/hw/inc"
INCLUDES="$INCLUDES -I$TT_METAL_HOME/build/libexec/tt-metalium/tt_metal/hw/inc/api/dataflow"

echo -e "${GREEN}[1/4] Compiling Blackhole I/O layer...${NC}"

# Compile blackhole_io.c
$RISCV_CC $CFLAGS $INCLUDES \
    -c src/zmachine/blackhole_io.c \
    -o "$BUILD_DIR/blackhole_io.o"

echo -e "${GREEN}[2/4] Compiling Frotz Z-machine core...${NC}"

# For proof of concept, let's compile just the essential Frotz files
# We'll add more as needed

# Generate Frotz headers if needed
FROTZ_SRC="src/zmachine/frotz/src"
if [ ! -f "$FROTZ_SRC/common/defs.h" ]; then
    echo "  Generating Frotz headers..."
    cat > "$FROTZ_SRC/common/defs.h" << 'EOF'
#ifndef COMMON_DEFINES_H
#define COMMON_DEFINES_H
#define VERSION "2.56pre-blackhole"
#define GIT_HASH "blackhole-poc"
#define GIT_DATE "2026-01-16"
#define RELEASE_NOTES "Blackhole RISC-V Proof of Concept"
#define UNIX
#define MAX_UNDO_SLOTS 0
#define MAX_FILE_NAME 80
#define TEXT_BUFFER_SIZE 512
#define INPUT_BUFFER_SIZE 200
#define STACK_SIZE 1024
#define USE_UTF8
#endif
EOF
    cat > "$FROTZ_SRC/common/hash.h" << 'EOF'
#ifndef COMMON_HASH_H
#define COMMON_HASH_H
#endif
EOF
fi

# Essential Frotz common files
FROTZ_COMMON_FILES=(
    "buffer.c"
    "err.c"
    "fastmem.c"
    "files.c"
    "input.c"
    "main.c"
    "math.c"
    "object.c"
    "process.c"
    "random.c"
    "redirect.c"
    "screen.c"
    "stream.c"
    "table.c"
    "text.c"
    "variable.c"
)

for file in "${FROTZ_COMMON_FILES[@]}"; do
    echo "  $file"
    $RISCV_CC $CFLAGS $INCLUDES \
        -c "$FROTZ_SRC/common/$file" \
        -o "$BUILD_DIR/frotz_$(basename ${file%.c}).o" || {
        echo -e "${YELLOW}Warning: Failed to compile $file, continuing...${NC}"
    }
done

echo -e "${GREEN}[3/4] Compiling Frotz dumb interface...${NC}"

# Dumb interface files (minimal terminal I/O)
FROTZ_DUMB_FILES=(
    "dinit.c"
    "dinput.c"
    "doutput.c"
)

for file in "${FROTZ_DUMB_FILES[@]}"; do
    echo "  $file"
    $RISCV_CC $CFLAGS $INCLUDES \
        -c "$FROTZ_SRC/dumb/$file" \
        -o "$BUILD_DIR/dumb_$(basename ${file%.c}).o" || {
        echo -e "${YELLOW}Warning: Failed to compile $file, continuing...${NC}"
    }
done

echo -e "${GREEN}[4/4] Compiling kernel entry point...${NC}"

# Compile kernel wrapper (C++ file)
RISCV_CXX="/opt/tenstorrent/sfpi/compiler/bin/riscv-tt-elf-g++"
$RISCV_CXX -std=c++17 $CFLAGS $INCLUDES \
    -c kernels/zork_kernel.cpp \
    -o "$BUILD_DIR/zork_kernel.o"

echo
echo -e "${GREEN}Build complete!${NC}"
echo "Kernel object files in: $BUILD_DIR/"
echo
echo "Next steps:"
echo "1. Link into TT-Metal program"
echo "2. Test on Blackhole hardware"
echo
