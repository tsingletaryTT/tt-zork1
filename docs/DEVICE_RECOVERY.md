# Device Recovery Guide

**Recovering from stuck firmware or device hangs**

## When Do You Need This?

You'll see errors like:
```
Device 0: Timeout (10000 ms) waiting for physical cores to finish: (x=1,y=2)
Device 0 init: failed to initialize FW! Try resetting the board.
```

This happens when:
- Long-running kernels hang the firmware
- Processes crash while using devices
- Multiple consecutive kernel invocations exceed firmware limits
- Device gets into an inconsistent state

## Recovery Methods (In Order of Preference)

### Method 1: Driver Reload (Fastest - ~10 seconds)

**Use the provided script:**
```bash
sudo ./scripts/tt-cold-reboot.sh
```

**What it does:**
1. Checks for processes using devices
2. Unloads `tenstorrent` kernel module
3. Reloads kernel module
4. Verifies devices are back

**When this works:**
- Software-level issues
- Stuck driver state
- Process cleanup needed

**When this doesn't work:**
- Firmware is genuinely hung
- Core-level timeout (like x=1,y=2)
- Hardware needs power cycle

**Success rate:** ~70% for typical issues

### Method 2: TT-SMI Reset (Medium - ~15 seconds)

**Reset specific device:**
```bash
tt-smi -r 0  # Reset device 0
tt-smi -r 1  # Reset device 1
```

**Reset all devices:**
```bash
tt-smi -r 0 1
```

**What it does:**
- Power cycles the specific PCIe device
- Resets firmware completely
- Clears all device state

**When this works:**
- Firmware timeouts
- Core-level hangs
- Most stuck states

**When this doesn't work:**
- Severe hardware issues
- Persistent firmware corruption
- PCIe bus problems

**Success rate:** ~95% for firmware issues

### Method 3: Power Cycle (Slowest - minutes)

**Full system reboot:**
```bash
sudo reboot
```

**Or physical power cycle:**
1. Save all work
2. Shut down system
3. Wait 10 seconds
4. Power back on

**When you need this:**
- tt-smi reset doesn't work
- Multiple devices stuck
- PCIe bus issues
- Last resort

**Success rate:** ~100% (always works)

## Troubleshooting Decision Tree

```
Device hang detected
    ↓
Try Method 1: Driver Reload (sudo ./scripts/tt-cold-reboot.sh)
    ↓
    Works? → Continue
    ↓ Fails
Try Method 2: TT-SMI Reset (tt-smi -r 0)
    ↓
    Works? → Continue
    ↓ Fails
Method 3: Power Cycle (sudo reboot)
```

## Prevention Strategies

### 1. Batch Size Limits

**Proven stable:** 5 batches of interpret(100)
- Total: 500 instructions
- Time: ~7 seconds
- Success rate: 100%

**Unstable:** 6+ batches
- Causes firmware hangs
- Requires reset
- Not recommended

**Recommendation:** Stick to 5 batches maximum

### 2. Device Persistence Best Practices

```cpp
// GOOD: Open once, use multiple times, close once
auto device = create_device();
for (int batch = 0; batch < 5; batch++) {
    run_kernel();
}
close_device();

// BAD: Open/close every time (slow but safer)
for (int batch = 0; batch < 5; batch++) {
    auto device = create_device();
    run_kernel();
    close_device();
}
```

### 3. Cleanup on Crash

Always wrap device code in try/catch:
```cpp
auto device = create_device();
try {
    run_kernel();
} catch (...) {
    close_device();  // Ensure cleanup!
    throw;
}
close_device();
```

### 4. Monitor for Hangs

Use timeouts on kernel execution:
```bash
timeout 60 ./your_program
```

If it times out, you know to reset before next run.

## Script Usage

### Basic Usage

```bash
# Simple reset
sudo ./scripts/tt-cold-reboot.sh
```

### Automated Recovery

```bash
# Try to run program, reset if it fails
if ! ./build-host/test_zork_optimized 5; then
    echo "Program failed, resetting device..."
    sudo ./scripts/tt-cold-reboot.sh
    echo "Retrying..."
    ./build-host/test_zork_optimized 5
fi
```

### In Makefiles

```makefile
.PHONY: reset run

reset:
\t@echo "Resetting Tenstorrent devices..."
\t@sudo ./scripts/tt-cold-reboot.sh

run: reset
\t@TT_METAL_RUNTIME_ROOT=/path/to/tt-metal ./build-host/program
```

## Common Error Messages

### "Device init: failed to initialize FW"

**Cause:** Firmware timeout, usually at a specific core

**Fix:** Method 2 (tt-smi reset) or Method 3 (power cycle)

**Not fixed by:** Method 1 (driver reload)

### "Timeout waiting for physical cores"

**Cause:** Specific core hung (e.g., x=1,y=2)

**Fix:** Method 2 (tt-smi reset)

**Prevention:** Reduce batch count, shorter kernel execution

### "Opening subset of mmio devices slows down UMD"

**Cause:** Using create_unit_mesh(0) instead of full mesh

**Fix:** Not an error, just a warning. Safe to ignore for single-device use.

**If it bothers you:** Use create_device() API instead

### "Downgrading to mesh shape 1x1"

**Cause:** Fabric connectivity issue (after driver reload or reset)

**Fix:** Usually resolves on next program run

**If persistent:** Physical reboot needed

## Zork-Specific Recommendations

### Current Proven Configuration

**What works reliably:**
- 5 batches × interpret(100)
- 500 total instructions
- ~7 seconds execution
- No resets needed

**What causes issues:**
- 6+ batches
- Hangs on batch 6
- Firmware timeout at core (x=1,y=2)

### Recovery Procedure for Zork

If test hangs:
1. Kill the process: `pkill -f test_zork`
2. Reset device: `tt-smi -r 0`
3. Wait 5 seconds
4. Run again with fewer batches

### Safe Testing Pattern

```bash
# Test incrementally
./build-host/test_zork_optimized 1   # Works
tt-smi -r 0 && sleep 5
./build-host/test_zork_optimized 2   # Works
tt-smi -r 0 && sleep 5
./build-host/test_zork_optimized 3   # Works
tt-smi -r 0 && sleep 5
./build-host/test_zork_optimized 5   # Works (proven)
tt-smi -r 0 && sleep 5
./build-host/test_zork_optimized 6   # May hang (reset after)
```

## Future Improvements

### What Would Help

1. **State persistence** - Save/restore Z-machine state between batches
2. **Smaller batches** - interpret(50) × 10 might work better than interpret(100) × 6
3. **Heartbeat monitoring** - Detect hangs earlier
4. **Graceful degradation** - Fall back to smaller batches on errors

### Firmware Limits

The 5-6 batch limit suggests:
- Firmware watchdog timeout ~5-6 seconds
- Resource accumulation (memory, handles?)
- Core state not fully clearing between kernels

**Tenstorrent may be able to:**
- Increase watchdog timeout
- Improve between-kernel cleanup
- Add firmware reset API

Until then, work within the 5-batch limit.

## Summary

**Quick Reference:**
- **Light issues:** `sudo ./scripts/tt-cold-reboot.sh`
- **Firmware hangs:** `tt-smi -r 0`
- **Severe issues:** `sudo reboot`
- **Prevent issues:** Stick to 5 batches maximum

**For Zork:**
- 5 batches = stable
- 6+ batches = unstable
- Reset between test runs when pushing limits

---

**When in doubt, use tt-smi reset!** It solves 95% of problems.
