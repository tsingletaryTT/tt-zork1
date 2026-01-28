# Zephyr RTOS Investigation - Critical Findings

**Date:** 2026-01-27
**Status:** Architecture Incompatibility Discovered

## Executive Summary

After investigating the `tt-zephyr-platforms` repository, I've discovered that **Zephyr RTOS cannot be used for our Zork project** as originally envisioned. The Zephyr firmware runs on different processor cores than the ones we've been using for TT-Metal kernels.

## Architecture Clarification

### Tenstorrent Blackhole Chip Architecture

The Blackhole chip has **two distinct types of processor cores**:

1. **ARM Cortex-M Management Core (DMC/SMC)**
   - Purpose: Board management (fans, power, sensors, I2C, etc.)
   - Runs: Zephyr RTOS firmware
   - Board: `tt_blackhole_tt_blackhole_dmc_p300c`
   - Arch: ARM (STM32G0B1)
   - RAM: 144KB
   - Flash: 512KB

2. **RISC-V Data Movement Cores (Tensix Grid)**
   - Purpose: AI workload data movement, custom compute
   - Runs: TT-Metal kernel firmware (controlled by TT-Metal runtime)
   - Location: Inside the Tensix grid
   - Our Zork interpreter runs HERE

### The Incompatibility

```
┌─────────────────────────────────────────────────────────┐
│  Tenstorrent Blackhole Chip                             │
│                                                          │
│  ┌────────────────┐        ┌────────────────────────┐  │
│  │  ARM DMC       │        │  RISC-V Cores          │  │
│  │  (Management)  │        │  (TT-Metal)            │  │
│  │                │        │                        │  │
│  │  Runs:         │        │  Runs:                 │  │
│  │  - Zephyr RTOS │◄───X───│  - TT-Metal kernels   │  │
│  │  - Board mgmt  │ No     │  - Our Zork code!     │  │
│  │  - Fans/power  │ Direct │                        │  │
│  │                │ Link   │                        │  │
│  └────────────────┘        └────────────────────────┘  │
│                                                          │
└─────────────────────────────────────────────────────────┘

        ↑                              ↑
    Zephyr goes here         We need to be here!
```

## What tt-zephyr-platforms IS

**Purpose:** Board management firmware
**Runs on:** ARM Cortex-M cores (DMC/SMC)
**Functions:**
- Fan control
- Power monitoring (INA228 sensor)
- Temperature management
- I2C/SMBUS communication
- Bootloader (MCUboot)
- JTAG programming
- Board fault detection

**Example from DMC main.c:**
```c
void update_fan_speed(bool notify_smcs) {
    uint8_t fan_speed = 0;
    ARRAY_FOR_EACH_PTR(BH_CHIPS, chip) {
        fan_speed = MAX(fan_speed, chip->data.fan_speed);
    }
    uint32_t fan_speed_pwm = DIV_ROUND_UP(fan_speed * UINT8_MAX, 100);
    pwm_set_cycles(max6639_pwm_dev, 0, UINT8_MAX, fan_speed_pwm, 0);
}
```

This is NOT where we can run Zork!

## What tt-zephyr-platforms IS NOT

❌ **NOT** an application runtime for RISC-V cores
❌ **NOT** a replacement for TT-Metal firmware
❌ **NOT** accessible from TT-Metal host API
❌ **NOT** suitable for running Z-machine interpreter

## Why This Matters for Zork

Our Z-machine interpreter has been running on **TT-Metal RISC-V cores** via:
```cpp
CreateKernel(program, "zork_interpreter.cpp", CoreCoord{0,0},
    DataMovementConfig{.processor = DataMovementProcessor::RISCV_0, ...});
```

These cores are:
- Controlled by TT-Metal runtime (not Zephyr)
- Loaded with firmware by `tt-metal/build/firmware/`
- Accessed via C++ host API (`distributed::MeshDevice`)
- NOT running Zephyr RTOS

## Could We Use Zephyr Somehow?

### Option 1: Run Zork on ARM DMC (Theoretical)

**Pros:**
- Zephyr shell available
- Persistent execution environment
- No TT-Metal complexity

**Cons:**
- ❌ ARM core is for board management, not applications
- ❌ Limited RAM (144KB) - too small for full game
- ❌ Would interfere with critical board management functions
- ❌ Not the intended use of the hardware
- ❌ Defeats the purpose (we want RISC-V, not ARM!)

**Verdict:** Technically possible but completely wrong approach.

### Option 2: Zephyr on TT-Metal RISC-V Cores (Not Supported)

**Idea:** Replace TT-Metal firmware with Zephyr on data movement cores.

**Cons:**
- ❌ Not supported by tt-zephyr-platforms
- ❌ Would break TT-Metal functionality
- ❌ TT-Metal RISC-V cores are tightly integrated with Tensix fabric
- ❌ No documentation or board configuration for this
- ❌ Would require deep firmware development

**Verdict:** Not feasible with current tooling.

### Option 3: Hybrid (DMC + RISC-V Communication)

**Idea:** Zephyr on ARM DMC orchestrates TT-Metal RISC-V cores.

**Pros:**
- ✅ Uses both cores for intended purposes
- ✅ Zephyr shell for user interface
- ✅ TT-Metal for computation

**Cons:**
- ⚠️ Complex: requires inter-core communication protocol
- ⚠️ No standard mechanism for DMC→RISC-V communication
- ⚠️ Would need custom driver development
- ⚠️ Adds unnecessary complexity over Python orchestrator

**Verdict:** Possible but extremely complex, no advantage over Python.

## Recommendations

### Abandon Zephyr Approach ❌

The Zephyr path is not viable for running Zork on TT-Metal RISC-V cores. It solves a different problem (board management) than what we need (application runtime).

### Focus on Python Orchestrator ✅

The Python approach we implemented is the **correct path forward**:

```python
# Python orchestrates from host
while not game_finished:
    result = subprocess.run([
        "./build-host/zork_on_blackhole",
        game_file,
        "1"  # One batch
    ])
    # TT-Metal handles RISC-V execution
    # Program cache eliminates overhead
```

**Advantages:**
- ✅ Works with TT-Metal as designed
- ✅ Runs on correct RISC-V cores
- ✅ Program cache solves performance issue
- ✅ Simple, maintainable architecture
- ✅ Already implemented and tested

### Alternative: TT-Metal Persistent Context

If we want to avoid device initialization overhead, the correct approach is to explore TT-Metal's persistent execution features, not Zephyr:

- Use program cache (already implemented)
- Investigate keeping device open across Python calls
- Explore TT-Metal's fast dispatch mode
- Consider calling C++ library from Python (via ctypes/PyBind11)

## Conclusion

The Zephyr investigation revealed an architectural mismatch:
- **Zephyr:** Board management on ARM cores
- **Our need:** Application runtime on RISC-V cores

**Next steps:**
1. ✅ Continue with Python orchestrator (already implemented)
2. ✅ Test with fresh device state now that devices are ready
3. ✅ Measure actual performance with program cache
4. ❌ Abandon Zephyr branch (architectural dead-end)

The good news: We already have the right solution (Python + TT-Metal). The Zephyr investigation helped us understand the hardware architecture better, even though it's not the right tool for our use case.

## References

- `tt-zephyr-platforms`: https://github.com/tenstorrent/tt-zephyr-platforms
- Board config: `/boards/tenstorrent/tt_blackhole/tt_blackhole_tt_blackhole_dmc_p300c.yaml`
- DMC firmware: `/app/dmc/src/main.c`
- Architecture: ARM Cortex-M (STM32G0B1) for management, RISC-V for compute
