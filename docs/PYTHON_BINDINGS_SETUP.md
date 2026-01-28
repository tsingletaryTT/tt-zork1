# Python Bindings Build Setup

**Date:** 2026-01-27
**Status:** ✅ SUCCESSFULLY BUILT

## Dependencies Required

### System Packages

```bash
# PyBind11 for C++/Python bindings
sudo apt-get install -y python3-pybind11 pybind11-dev

# libfmt for TT-Metalium
sudo apt-get install -y libfmt-dev

# Python development headers (usually already installed)
sudo apt-get install -y python3-dev
```

### TT-Metal CMake Dependencies

The build requires finding several TT-Metal CMake configuration files. These must be passed as CMake variables:

1. **TT-Metalium** (main library)
   - Path: `/home/ttuser/tt-metal/build_Release/lib/cmake/tt-metalium`
   - Variable: `-DTT-Metalium_DIR`

2. **umd** (Unified Memory Driver)
   - Path: `/home/ttuser/tt-metal/build_Release/lib/cmake/umd`
   - Variable: `-Dumd_DIR`

3. **spdlog** (logging library)
   - Path: `/home/ttuser/tt-metal/build_Release/_deps/spdlog-build`
   - Variable: `-Dspdlog_DIR`

4. **tt-logger** (TT logging wrapper)
   - Path: `/home/ttuser/tt-metal/build_Release/_deps/tt-logger-build/cmake`
   - Variable: `-Dtt-logger_DIR`

## Build Instructions

### Step 1: Navigate to python_bindings directory

```bash
cd /home/ttuser/code/tt-zork1/python_bindings
mkdir -p build && cd build
```

### Step 2: Configure with CMake

```bash
cmake .. \
  -DTT-Metalium_DIR=/home/ttuser/tt-metal/build_Release/lib/cmake/tt-metalium \
  -Dumd_DIR=/home/ttuser/tt-metal/build_Release/lib/cmake/umd \
  -Dspdlog_DIR=/home/ttuser/tt-metal/build_Release/_deps/spdlog-build \
  -Dtt-logger_DIR=/home/ttuser/tt-metal/build_Release/_deps/tt-logger-build/cmake
```

**Expected output:**
```
-- Found pybind11: /usr/include (found version "2.11.1")
-- Found umd at /home/ttuser/tt-metal/build_Release/lib/cmake/umd
-- Found TT-Metalium at /home/ttuser/tt-metal/build_Release/lib/cmake/tt-metalium
-- Configuring done (0.0s)
-- Generating done (0.0s)
-- Build files have been written to: /home/ttuser/code/tt-zork1/python_bindings/build
```

### Step 3: Build

```bash
make -j$(nproc)
```

**Expected output:**
```
[ 50%] Building CXX object CMakeFiles/zork_tt.dir/zork_bindings.cpp.o
[100%] Linking CXX shared module zork_tt.cpython-312-x86_64-linux-gnu.so
[100%] Built target zork_tt
```

### Step 4: Install

```bash
make install
```

This installs the Python module to `/home/ttuser/code/tt-zork1/python_bindings/zork_tt.cpython-312-x86_64-linux-gnu.so`

## Verification

### Test import:

```bash
cd /home/ttuser/code/tt-zork1

LD_LIBRARY_PATH=/home/ttuser/tt-metal/build_Release/lib:$LD_LIBRARY_PATH \
python3 -c "import sys; sys.path.insert(0, 'python_bindings'); import zork_tt; print('Success!'); print(dir(zork_tt))"
```

**Expected output:**
```
Module imported!
Functions: ['close_device', 'execute_batch', 'get_state', 'init_device', 'load_game', 'set_state']
```

## Usage

### From Python:

```python
import sys
sys.path.insert(0, 'python_bindings')
import zork_tt

# Initialize device
device = zork_tt.init_device()

# Load game file
zork_tt.load_game(device, "game/zork1.z3")

# Execute 100 Z-machine instructions
output = zork_tt.execute_batch(device, 100)
print(output)

# Close device
zork_tt.close_device(device)
```

### Using wrapper script:

```bash
./run-zork-persistent.sh
```

This script:
- Sets `LD_LIBRARY_PATH` for TT-Metal libraries
- Sets `TT_METAL_RUNTIME_ROOT` environment variable
- Runs `zork_persistent.py`

## Troubleshooting

### Error: "libtt_metal.so: cannot open shared object file"

**Solution:** Set `LD_LIBRARY_PATH` before running Python:
```bash
export LD_LIBRARY_PATH=/home/ttuser/tt-metal/build_Release/lib:$LD_LIBRARY_PATH
```

### Error: "ImportError: No module named 'zork_tt'"

**Solution:** Add python_bindings to Python path:
```python
import sys
sys.path.insert(0, 'python_bindings')
```

### CMake Error: "Could not find package configuration file"

**Solution:** Ensure all `-D` variables are set correctly in cmake command. Check paths exist:
```bash
ls /home/ttuser/tt-metal/build_Release/lib/cmake/tt-metalium/tt-metalium-config.cmake
ls /home/ttuser/tt-metal/build_Release/lib/cmake/umd/umdConfig.cmake
ls /home/ttuser/tt-metal/build_Release/_deps/spdlog-build/spdlogConfig.cmake
ls /home/ttuser/tt-metal/build_Release/_deps/tt-logger-build/cmake/tt-logger-config.cmake
```

## Architecture

The Python bindings provide a persistent device context:

```
Python Process (lives entire session)
    ↓
zork_tt.init_device()  # Device initialized ONCE
    ↓
Device stays open
    ↓
Multiple calls to zork_tt.execute_batch()  # Fast! Program cache active
    ↓
zork_tt.close_device()  # Device closed ONCE at end
```

**Key Advantages:**
- No repeated device initialization overhead
- Program cache active across all batches
- Expected performance: ~0.5s per batch (vs 2-3s with device reset)

## Files

- `python_bindings/zork_bindings.cpp` - PyBind11 C++ module source
- `python_bindings/CMakeLists.txt` - Build configuration
- `python_bindings/build/` - Build directory (generated)
- `python_bindings/zork_tt.cpython-312-x86_64-linux-gnu.so` - Compiled Python module
- `zork_persistent.py` - Python script using the bindings
- `run-zork-persistent.sh` - Wrapper script with environment setup

## Build Time

- Configuration: <1 second
- Compilation: ~3-5 seconds on modern CPU
- Total: ~5-10 seconds for clean build

## Module Size

Compiled module: ~475KB

## Python Version

Tested with Python 3.12.3. Compatible with Python 3.8+.
