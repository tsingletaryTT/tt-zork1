# Zephyr RTOS Approach for Zork on Blackhole

## Overview

This document outlines the strategy for running Zork I on Tenstorrent Blackhole using Zephyr RTOS as a persistent runtime environment on RISC-V cores.

## Why Zephyr?

**Current Problem:**
- C++ TT-Metal programs must close device after execution
- Device initialization adds 2-3 seconds overhead per run
- Program cache helps but full Z-machine interpreter still times out
- State persistence between separate program invocations is unreliable

**Zephyr Solution:**
- Persistent RTOS running on RISC-V cores
- No repeated device initialization overhead
- True persistent execution environment
- Shell interface for interactive I/O
- Native RISC-V environment (not orchestrated from host)

## Architecture

```
┌─────────────────────────────────────────────────┐
│  Host (x86)                                     │
│  - Serial console connection                    │
│  - Sends user commands                          │
│  - Receives game output                         │
└────────────────┬────────────────────────────────┘
                 │ UART/Serial
┌────────────────┴────────────────────────────────┐
│  Blackhole RISC-V Core 0 (Zephyr RTOS)         │
│  ┌────────────────────────────────────────────┐ │
│  │  Zephyr Shell                              │ │
│  │  - User command interface                  │ │
│  │  - Zork-specific commands                  │ │
│  └────────────┬───────────────────────────────┘ │
│               │                                  │
│  ┌────────────┴───────────────────────────────┐ │
│  │  Z-machine Interpreter                     │ │
│  │  - Game file in DRAM                       │ │
│  │  - Persistent state in memory              │ │
│  │  - Executes opcodes continuously           │ │
│  └────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────┘
```

## Zephyr Application Structure

Based on tt-zephyr-platforms repository, a typical Zephyr app for Tenstorrent:

```
zork-zephyr/
├── prj.conf              # Zephyr configuration
├── CMakeLists.txt        # Zephyr build system
├── src/
│   ├── main.c            # Application entry point
│   ├── zmachine.c        # Z-machine interpreter (ported from kernels/)
│   ├── zork_shell.c      # Shell commands for interaction
│   └── io_adapter.c      # I/O layer for Zephyr console
└── boards/
    └── blackhole.overlay # Device tree overlay for Blackhole
```

## Implementation Plan

### Phase 1: Zephyr Environment Setup
**Goal:** Get basic Zephyr "Hello World" running on Blackhole RISC-V core

1. Clone and study tt-zephyr-platforms repository
2. Set up Zephyr SDK and build environment
3. Build and flash sample Zephyr application
4. Verify serial console communication works
5. Test Zephyr shell functionality

**Success Criteria:**
- Can flash Zephyr image to Blackhole
- Can connect to serial console
- Shell responds to commands
- "Hello World" prints from RISC-V core

### Phase 2: Z-machine Interpreter Port
**Goal:** Port existing Z-machine C++ kernel to C for Zephyr

1. Convert kernels/zork_interpreter.cpp to C
2. Replace NoC APIs with Zephyr memory access
3. Adapt to Zephyr threading model (if needed)
4. Implement game file loading from DRAM
5. Test with interpret(100) execution

**Success Criteria:**
- Z-machine code compiles in Zephyr environment
- Can load game file from DRAM
- Interpreter executes opcodes
- Output appears in console

### Phase 3: Shell Integration
**Goal:** Create interactive Zork shell commands

Zephyr shell commands:
```
zork> start                    # Initialize game
zork> step [n]                # Execute n instructions (default: 100)
zork> continue                # Execute until input needed
zork> input "north"           # Send command to game
zork> status                  # Show Z-machine state (PC, stack)
zork> save <file>             # Save state to file
zork> load <file>             # Load state from file
```

**Success Criteria:**
- Shell commands work reliably
- Can play Zork interactively via shell
- State persists across commands
- No device resets needed

### Phase 4: Continuous Execution
**Goal:** Full interactive gameplay loop

1. Implement event-driven execution model
2. Execute Z-machine until READ opcode (input request)
3. Wait for user input via shell
4. Resume execution with input
5. Display output continuously

**Success Criteria:**
- Full Zork gameplay works
- Response time acceptable (<5 seconds per turn)
- No crashes or hangs
- Can complete game from start to finish

## Technical Considerations

### Memory Management
- **Game file:** Load once to DRAM (128KB)
- **Z-machine state:** Keep in SRAM for fast access (~16KB)
- **Output buffer:** Circular buffer in SRAM (16KB)
- **Stack:** Zephyr manages thread stack

### Threading Model
Options:
1. **Single-threaded:** Z-machine runs in main thread, blocks on input
2. **Two-threaded:** Separate threads for shell and interpreter
3. **Event-driven:** Use Zephyr work queues for non-blocking execution

Recommendation: Start with single-threaded (simplest), move to event-driven if needed.

### I/O Abstraction
Zephyr provides standard console I/O:
```c
#include <zephyr/console/console.h>

// Output
printk("You are standing in an open field...\n");

// Input
char *line = console_getline();
```

### Performance Expectations
- **No device init overhead:** Zephyr runs continuously
- **Instruction execution:** Similar to current ~100 instructions/batch
- **Response time:** Expected <1 second per turn (no reset overhead)
- **Memory:** Fits easily in available SRAM

## Advantages Over Python Orchestrator

| Feature | Zephyr RTOS | Python Orchestrator |
|---------|-------------|---------------------|
| **Initialization overhead** | Once (at boot) | Every kernel invocation |
| **State persistence** | Native memory | File I/O or buffers |
| **Execution model** | Continuous | Batched (100 instructions) |
| **Response time** | <1 second | 4-15 seconds (with cache) |
| **Complexity** | High (RTOS port) | Medium (Python + kernel) |
| **Debugging** | Serial console | Host-side logs |
| **Interactive I/O** | Native shell | Python input() |

## Disadvantages

- **Higher initial investment:** Requires Zephyr expertise
- **Hardware-specific:** Must port to each board type
- **Debugging complexity:** Serial console debugging is harder
- **Firmware flashing:** Requires proper flash tooling

## Comparison to Current Approach

### Current (TT-Metal C++):
```
Host: Create device → Load data → Execute kernel → Read output → Close device
Time: 2-3 seconds overhead + ~0.1s execution = 2.1-3.1s per batch
State: Save to file between runs
```

### Zephyr RTOS:
```
Boot: Initialize Zephyr → Load game once → Enter shell
Per turn: Execute until input needed (~0.1-0.5s) → Display output → Get input
State: Persistent in memory (no file I/O)
```

## Next Steps

1. **Research Phase (Current):**
   - Study tt-zephyr-platforms repository structure
   - Identify required Zephyr modules (console, shell, memory)
   - Understand board configuration for Blackhole

2. **Proof of Concept:**
   - Get basic Zephyr app running on Blackhole
   - Verify serial console works
   - Test memory access patterns (DRAM, SRAM)

3. **Port Interpreter:**
   - Convert C++ to C
   - Replace TT-Metal APIs with Zephyr equivalents
   - Test with simple game commands

4. **Full Integration:**
   - Build complete shell interface
   - Implement continuous execution loop
   - Achieve playable Zork

## Resources

- **tt-zephyr-platforms:** https://github.com/tenstorrent/tt-zephyr-platforms
- **Zephyr Shell Guide:** https://docs.zephyrproject.org/latest/services/shell/index.html
- **Zephyr Console:** https://docs.zephyrproject.org/latest/services/console/index.html
- **RISC-V Zephyr Boards:** https://docs.zephyrproject.org/latest/boards/riscv/index.html

## Decision: Python vs Zephyr?

**Python Orchestrator:**
- ✅ Faster to implement (days)
- ✅ Easier debugging
- ✅ Leverages existing Python tools
- ⚠️ Slower execution (batched)
- ⚠️ More orchestration complexity

**Zephyr RTOS:**
- ✅ Better performance (continuous execution)
- ✅ True embedded experience
- ✅ No host orchestration needed
- ⚠️ Longer implementation (weeks)
- ⚠️ Harder debugging (serial console)

**Recommendation:** Implement Python first for proof-of-concept, then Zephyr for production/performance if needed. Python gets us to playable Zork faster, Zephyr makes it elegant.
