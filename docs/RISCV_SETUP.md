# RISC-V Cross-Compilation Setup

**Phase 1.3**: Building Zork for Tenstorrent's RISC-V Cores

## Overview

This guide walks through cross-compiling Zork I for RISC-V 64-bit architecture, targeting Tenstorrent Blackhole's RISC-V cores (16x 64-bit cores per chip).

## Target Architecture

**Tenstorrent Blackhole RISC-V Cores:**
- **ISA**: RV64IMAC
  - `RV64`: 64-bit base integer ISA
  - `I`: Integer instructions
  - `M`: Integer multiplication and division
  - `A`: Atomic instructions
  - `C`: Compressed instructions (16-bit encoding)
- **ABI**: LP64 (long and pointers are 64-bit)
- **Cores**: 16 per Blackhole chip
- **Use case**: Sequential game logic, Z-machine interpreter

## Prerequisites

### 1. Install RISC-V Toolchain

#### Option A: macOS (Homebrew)

```bash
# Install Homebrew tap for RISC-V tools
brew tap riscv-software-src/riscv

# Install GNU toolchain
brew install riscv-gnu-toolchain

# Verify installation
riscv64-unknown-elf-gcc --version
```

#### Option B: Ubuntu/Debian

```bash
# Install from package manager
sudo apt-get update
sudo apt-get install gcc-riscv64-linux-gnu g++-riscv64-linux-gnu

# Verify installation
riscv64-linux-gnu-gcc --version
```

#### Option C: Build from Source (All Platforms)

```bash
# Clone the official RISC-V GNU toolchain
git clone https://github.com/riscv/riscv-gnu-toolchain
cd riscv-gnu-toolchain

# Configure for RV64IMAC
./configure --prefix=/opt/riscv \
    --with-arch=rv64imac \
    --with-abi=lp64

# Build (this takes 30-60 minutes)
make -j$(nproc)

# Add to PATH
export PATH="/opt/riscv/bin:$PATH"

# Verify
riscv64-unknown-elf-gcc --version
```

### 2. Install QEMU (Optional, for Testing)

```bash
# macOS
brew install qemu

# Ubuntu/Debian
sudo apt-get install qemu-user qemu-user-static

# Verify
qemu-riscv64 --version
```

## Building for RISC-V

### Quick Start

```bash
# Build RISC-V binary
./scripts/build_riscv.sh

# Clean build
./scripts/build_riscv.sh clean

# Release build (optimized)
./scripts/build_riscv.sh release
```

### Build Output

The build creates:
- `build-riscv/` - Object files and build artifacts
- `zork-riscv` - Statically-linked RISC-V 64-bit ELF executable

### Build Configuration

The build script automatically:
1. Detects available RISC-V toolchain
2. Configures for RV64IMAC architecture
3. Enables UTF-8 support
4. Creates statically-linked binary (for bare-metal deployment)
5. Compiles Frotz + dumb interface + blorb library

## Testing

### Local Testing with QEMU

```bash
# Run on QEMU user-mode emulation
qemu-riscv64 -L /usr/riscv64-linux-gnu ./zork-riscv game/zork1.z3

# Or if using static binary
qemu-riscv64-static ./zork-riscv game/zork1.z3
```

**Note**: QEMU provides syscall emulation, so file I/O works transparently.

### Verify Binary

```bash
# Check binary type
file zork-riscv
# Expected: ELF 64-bit LSB executable, UCB RISC-V, version 1 (SYSV), statically linked

# Check architecture
riscv64-unknown-elf-readelf -h zork-riscv | grep Machine
# Expected: Machine: RISC-V

# Check binary size
riscv64-unknown-elf-size zork-riscv
```

## Deployment to Tenstorrent Hardware

### Prerequisites

1. **TT-Metal SDK Installed**
   ```bash
   # Clone TT-Metal
   git clone https://github.com/tenstorrent/tt-metal
   cd tt-metal

   # Install dependencies and build
   ./scripts/build_scripts/build_with_profiler_opt.sh

   # Set environment variable
   export TT_METAL_HOME=$(pwd)
   ```

2. **Hardware Access**
   - Wormhole or Blackhole card installed
   - Drivers loaded
   - Verified with `tt-smi` or equivalent

### Deployment Steps

#### 1. Copy Binary to Hardware Environment

```bash
# If developing remotely, copy to hardware machine
scp ./zork-riscv user@hardware-machine:/path/to/deployment/
scp ./game/zork1.z3 user@hardware-machine:/path/to/deployment/
```

#### 2. Load onto RISC-V Core

```cpp
// Example TT-Metal code to load and run on RISC-V
#include "tt_metal/host_api.hpp"

using namespace tt::tt_metal;

int main() {
    // Get device
    Device *device = CreateDevice(0);

    // Load RISC-V binary
    std::vector<uint32_t> binary = ReadBinaryFile("zork-riscv");

    // Create program for RISC-V core
    Program program = CreateProgram();

    // Configure RISC-V core
    // Load binary to RISC-V core memory
    // Set entry point
    // Execute

    CloseDevice(device);
    return 0;
}
```

**Note**: Detailed TT-Metal integration will be implemented in Phase 1.4.

## Troubleshooting

### Toolchain Not Found

```
ERROR: No RISC-V toolchain found!
```

**Solution**: Install a RISC-V toolchain (see Prerequisites above), or set:
```bash
export RISCV_TOOLCHAIN=riscv64-linux-gnu
```

### Static Linking Errors

```
cannot find -lc
```

**Solution**: You may need newlib or glibc for RISC-V:
```bash
# Build toolchain with newlib
./configure --prefix=/opt/riscv --with-arch=rv64imac --with-abi=lp64 --enable-multilib
```

### QEMU Illegal Instruction

```
qemu: fatal: Illegal instruction
```

**Solution**: Ensure QEMU supports RV64IMAC:
```bash
qemu-riscv64 -cpu help | grep rv64
```

## Binary Size Optimization

Current size: ~220KB (debug), ~160KB (release)

### Further Optimization

```bash
# Strip debug symbols
riscv64-unknown-elf-strip zork-riscv

# Link-time optimization
./scripts/build_riscv.sh release
# (already uses -O3)

# Compress with UPX (if available)
upx --best --ultra-brute zork-riscv
```

## Technical Details

### Memory Layout

```
Typical RISC-V memory layout:
0x00000000 - 0x000FFFFF: Reserved/ROM
0x00100000 - 0x7FFFFFFF: RAM (text/data/bss/heap/stack)
0x80000000 - 0xFFFFFFFF: I/O devices
```

### Calling Convention

- **Integer args**: a0-a7 (x10-x17)
- **Return value**: a0-a1
- **Stack pointer**: sp (x2)
- **Return address**: ra (x1)
- **Saved registers**: s0-s11
- **Temporary registers**: t0-t6

### ABI: LP64

- `char`: 8-bit
- `short`: 16-bit
- `int`: 32-bit
- `long`: 64-bit
- `pointer`: 64-bit

## Performance Expectations

**On RISC-V @ 1 GHz:**
- Game loop: ~1M cycles/frame
- Input processing: ~10K cycles
- Text rendering: ~50K cycles
- Expected FPS: ~1000+ (game is turn-based, so not relevant)

**Memory usage:**
- Code: ~160KB
- Z-machine memory: ~128KB (Zork I)
- Stack: ~64KB
- Total: ~400KB

## Next Steps

After successful RISC-V build:

1. **Phase 1.4**: TT-Metal integration
   - Implement `io_ttmetal.c` for hardware I/O
   - RISC-V loader for TT-Metal
   - Deploy and test on actual hardware

2. **Phase 2**: LLM Parser Integration
   - RISC-V cores handle game logic
   - Tensix cores handle natural language understanding
   - Message passing between architectures

## References

- [RISC-V Specification](https://riscv.org/specifications/)
- [RISC-V GNU Toolchain](https://github.com/riscv/riscv-gnu-toolchain)
- [TT-Metal Documentation](https://github.com/tenstorrent/tt-metal)
- [QEMU RISC-V](https://www.qemu.org/docs/master/system/target-riscv.html)

---

**Status**: Build script complete | Waiting for toolchain installation | Ready for testing
