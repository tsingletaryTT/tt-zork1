# Static Compilation Options for TT-Metal

**Question:** Can we compile TT-Metal requirements statically so the binary doesn't need environment variables?

**Answer:** Partially yes, but with limitations. Here are the options:

---

## Why TT_METAL_RUNTIME_ROOT is Needed

The environment variable isn't just for linking - it's needed at **runtime** for:

1. **Firmware Binaries** - `/firmware/*.hex` files loaded to device during initialization
2. **Kernel JIT Compilation** - RISC-V kernels compile on-the-fly when program runs
3. **Device Configuration** - YAML files with hardware specs, memory maps
4. **UMD Resources** - User-mode driver needs runtime config files

You can verify this:
```bash
ls $TT_METAL_RUNTIME_ROOT/
# firmware/  kernels/  runtime/  etc/
```

---

## Option 1: Auto-Detection Script (✅ Current Solution)

**Status:** Implemented in `play-zork-interactive.sh`

**How it works:**
```bash
# Auto-detects common TT-Metal locations
if [ -d "/home/ttuser/tt-metal" ]; then
    export TT_METAL_RUNTIME_ROOT=/home/ttuser/tt-metal
elif [ -d "$HOME/tt-metal" ]; then
    export TT_METAL_RUNTIME_ROOT=$HOME/tt-metal
fi
```

**Pros:**
- ✅ Works immediately
- ✅ No code changes needed
- ✅ Portable across machines

**Cons:**
- ⚠️ Still needs TT-Metal installed at runtime
- ⚠️ Script required (can't just run binary directly)

**Best for:** Quick testing, development, demos

---

## Option 2: Compile-Time Path Embedding

**Implementation:**

Add to `CMakeLists.txt`:
```cmake
# Get TT-Metal path at compile time
if(DEFINED ENV{TT_METAL_RUNTIME_ROOT})
    set(TT_METAL_ROOT $ENV{TT_METAL_RUNTIME_ROOT})
else()
    set(TT_METAL_ROOT "/home/ttuser/tt-metal")
endif()

# Embed as compile-time define
target_compile_definitions(zork_on_blackhole PRIVATE
    TT_METAL_ROOT_PATH="${TT_METAL_ROOT}"
)
```

Add to `zork_on_blackhole.cpp`:
```cpp
#ifndef TT_METAL_ROOT_PATH
#define TT_METAL_ROOT_PATH "/home/ttuser/tt-metal"
#endif

int main() {
    // Set environment variable from compile-time constant
    setenv("TT_METAL_RUNTIME_ROOT", TT_METAL_ROOT_PATH, 0);

    // Rest of program...
}
```

**Pros:**
- ✅ No script needed - binary sets its own environment
- ✅ Can run binary directly: `./zork_on_blackhole`
- ✅ Path baked in at compile time

**Cons:**
- ⚠️ Hardcodes path - not portable to other machines
- ⚠️ Still needs TT-Metal installed at that path
- ⚠️ Recompile if TT-Metal moves

**Best for:** Single-machine deployment, kiosks, embedded systems

---

## Option 3: Bundle Resources in Binary

**Concept:** Embed firmware/configs as binary data

**Implementation (Advanced):**
```bash
# Convert firmware to C array
xxd -i firmware.hex > firmware_data.c
```

```cpp
// In zork_on_blackhole.cpp
extern unsigned char firmware_data[];
extern unsigned int firmware_data_len;

// Extract at runtime to /tmp/
void extract_resources() {
    FILE* f = fopen("/tmp/tt-metal-firmware.hex", "wb");
    fwrite(firmware_data, 1, firmware_data_len, f);
    fclose(f);
    setenv("TT_METAL_RUNTIME_ROOT", "/tmp/tt-metal-extracted", 1);
}
```

**Pros:**
- ✅ Truly self-contained binary
- ✅ No external dependencies at runtime
- ✅ Can ship as single file

**Cons:**
- ⚠️ Large binary size (firmware is ~10MB+)
- ⚠️ Needs write access to /tmp/
- ⚠️ Complex to maintain (rebundle on TT-Metal updates)
- ⚠️ Kernel JIT still needs compiler toolchain

**Best for:** Distributable applications, offline installations

---

## Option 4: Kernel Pre-Compilation (Experimental)

**Concept:** Pre-compile RISC-V kernels instead of JIT

TT-Metal normally compiles kernels on-the-fly. We could pre-compile:

```bash
# Pre-compile kernel
tt-metal-compile \
    kernels/zork_interpreter_opt.cpp \
    -o kernels/zork_interpreter_opt.bin

# Load pre-compiled binary at runtime
# (Requires TT-Metal API support)
```

**Status:** Not currently supported by TT-Metal APIs (as of 2026-01)

**Pros:**
- ✅ Faster startup (no compile time)
- ✅ Could bundle kernel binaries with application
- ✅ Reduced dependencies

**Cons:**
- ⚠️ Not officially supported yet
- ⚠️ Would need TT-Metal API changes
- ⚠️ Still needs firmware and device configs

**Best for:** Future optimization

---

## Option 5: Docker Container (Practical Alternative)

**Concept:** Bundle everything in a container

```dockerfile
FROM ubuntu:22.04

# Install TT-Metal
COPY tt-metal/ /opt/tt-metal/
ENV TT_METAL_RUNTIME_ROOT=/opt/tt-metal

# Copy Zork binary
COPY build-host/zork_on_blackhole /usr/local/bin/
COPY game/zork1.z3 /opt/zork/

# Device access
RUN usermod -aG render,video $USER

CMD ["/usr/local/bin/zork_on_blackhole", "/opt/zork/zork1.z3", "5"]
```

**Pros:**
- ✅ Fully reproducible environment
- ✅ Easy distribution
- ✅ Works on any machine with Docker + TT hardware
- ✅ Isolates dependencies

**Cons:**
- ⚠️ Requires Docker
- ⚠️ Device passthrough complexity
- ⚠️ Container overhead

**Best for:** Production deployment, cloud environments

---

## Recommended Approach

### For Development (Now)
✅ **Use Option 1** - Auto-detection script (already implemented!)

The script is now self-contained enough that users don't need to manually set anything. Just run:
```bash
./play-zork-interactive.sh
```

### For Single-Machine Deployment
**Implement Option 2** - Compile-time path embedding

Add 10 lines to CMakeLists.txt and 3 lines to main():
```cpp
// At start of main()
#ifndef TT_METAL_ROOT_PATH
#define TT_METAL_ROOT_PATH "/home/ttuser/tt-metal"
#endif
setenv("TT_METAL_RUNTIME_ROOT", TT_METAL_ROOT_PATH, 0);
```

Then binary works standalone:
```bash
./zork_on_blackhole game/zork1.z3 5
```

### For Distribution
**Use Option 5** - Docker container

Bundles everything, works anywhere, easy to ship.

---

## Example: Implementing Option 2 (Compile-Time Path)

Want to make the binary truly standalone? Here's the exact changes:

### 1. Update CMakeLists.txt
```cmake
# After find_package(TT-Metalium)
if(DEFINED ENV{TT_METAL_RUNTIME_ROOT})
    set(TT_METAL_ROOT $ENV{TT_METAL_RUNTIME_ROOT})
else()
    set(TT_METAL_ROOT "/home/ttuser/tt-metal")
endif()

message(STATUS "Embedding TT-Metal path: ${TT_METAL_ROOT}")

# For each executable:
target_compile_definitions(zork_on_blackhole PRIVATE
    TT_METAL_ROOT_PATH="${TT_METAL_ROOT}"
)
```

### 2. Update zork_on_blackhole.cpp
```cpp
int main(int argc, char* argv[]) {
    // Set TT-Metal path if not already set
    #ifdef TT_METAL_ROOT_PATH
    if (!getenv("TT_METAL_RUNTIME_ROOT")) {
        setenv("TT_METAL_RUNTIME_ROOT", TT_METAL_ROOT_PATH, 1);
        std::cout << "Using embedded TT-Metal path: "
                  << TT_METAL_ROOT_PATH << std::endl;
    }
    #endif

    // Rest of existing main()...
}
```

### 3. Rebuild
```bash
cd build-host
cmake --build . --parallel
```

### 4. Test
```bash
# No environment variable needed!
./build-host/zork_on_blackhole game/zork1.z3 5
```

---

## Summary

| Option | Standalone? | Portable? | Complexity | Status |
|--------|-------------|-----------|------------|--------|
| Script | No | Yes | Low | ✅ Done |
| Embedded Path | Yes | No | Low | Ready to implement |
| Bundle Resources | Yes | Yes | High | Possible but complex |
| Pre-compile Kernels | Yes | Yes | Medium | Not yet supported |
| Docker | Yes | Yes | Medium | Best for production |

**Recommendation:**
- **Now:** Use the script (Option 1) ✅
- **Next:** Implement embedded path (Option 2) for convenience
- **Future:** Consider Docker (Option 5) for distribution

The reality is that TT-Metal is a **hardware abstraction layer** that needs runtime resources. Truly static compilation isn't possible without TT-Metal's architecture changing. The practical solutions above work around this limitation.

---

**Want me to implement Option 2 (embedded path) for you?** It's just 15 lines of code and would make the binary work standalone on the current machine.
