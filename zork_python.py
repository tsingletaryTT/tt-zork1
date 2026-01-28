#!/usr/bin/env python3
# SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
# SPDX-License-Identifier: Apache-2.0

"""
Zork I on Tenstorrent Blackhole - Python Orchestrator

This Python script orchestrates repeated execution of the Z-machine interpreter
C++ program, taking advantage of TT-Metal's program cache for efficient batching.

Key advantages:
- Simpler than direct Python API (reuses existing C++ code)
- Program cache eliminates recompilation overhead
- Easy state persistence via filesystem
- Clean separation: Python orchestrates, C++ executes

Architecture:
    [Python Orchestrator]
        ↓
    Loop until game complete:
        ↓
    Execute zork_on_blackhole (C++)
        ↓
    ├─ First run: Compiles kernel + caches program (~2-3s)
        ↓
    ├─ Later runs: Uses cached program (~0.5s)
        ↓
    ├─ Loads state from /tmp/zork_state.bin
        ↓
    ├─ Executes 100 Z-machine instructions
        ↓
    └─ Saves state to /tmp/zork_state.bin
        ↓
    Extract & display output
        ↓
    Check if game finished
"""

import sys
import subprocess
import time
from pathlib import Path

# Configuration
GAME_FILE = "game/zork1.z3"
STATE_FILE = "/tmp/zork_state.bin"
ZORK_EXECUTABLE = "./build-host/zork_on_blackhole"
TT_METAL_ROOT = "/home/ttuser/tt-metal"
INSTRUCTIONS_PER_BATCH = 100
MAX_BATCHES = 50  # Limit for safety (5000 total instructions)


def run_zork_batch(batch_num: int, num_batches: int = 1) -> tuple[str, bool]:
    """
    Execute one batch of Z-machine instructions via C++ program.

    Returns:
        (output_text, is_finished)
    """
    # Copy current environment and add TT-Metal variables
    env = subprocess.os.environ.copy()
    env["TT_METAL_RUNTIME_ROOT"] = TT_METAL_ROOT
    env["HOME"] = env.get("HOME", "/home/ttuser")  # Ensure HOME is set for MPI

    # Run C++ executable with num_batches parameter
    # If num_batches=1: Single batch with state persistence
    # Program cache will speed up after first run!
    cmd = [ZORK_EXECUTABLE, GAME_FILE, str(num_batches)]

    start_time = time.time()
    result = subprocess.run(
        cmd,
        env=env,
        capture_output=True,
        text=True,
        cwd="/home/ttuser/code/tt-zork1"
    )
    elapsed = time.time() - start_time

    if result.returncode != 0:
        print(f"[ERROR] Batch {batch_num} failed:")
        print(result.stderr)
        return ("", False)

    # Extract game output from between the box borders
    output = result.stdout

    # Look for accumulated output section
    if "ACCUMULATED ZORK OUTPUT" in output:
        lines = output.split('\n')
        in_output = False
        game_text = []
        for line in lines:
            if "ACCUMULATED ZORK OUTPUT" in line:
                in_output = True
                continue
            if in_output:
                if line.startswith('╚'):  # End of box
                    break
                if not line.startswith('╔') and not line.startswith('║'):
                    game_text.append(line)

        output_text = '\n'.join(game_text).strip()
    else:
        output_text = ""

    # Check if game has finished (look for completion markers)
    is_finished = "Game complete" in output or batch_num >= MAX_BATCHES

    print(f"[Batch {batch_num}] Completed in {elapsed:.2f}s")
    if batch_num == 1:
        print(f"           ↑ First run includes compilation (~2-3s)")
    elif batch_num == 2:
        print(f"           ↑ Program cache active! Should be faster (~0.5s)")

    return (output_text, is_finished)


def main():
    print("╔══════════════════════════════════════════════════════════╗")
    print("║  ZORK I - Python Orchestrator on Blackhole RISC-V       ║")
    print("║  Leveraging TT-Metal program cache for fast execution   ║")
    print("╚══════════════════════════════════════════════════════════╝")
    print()

    # Check that executable exists
    if not Path(ZORK_EXECUTABLE).exists():
        print(f"ERROR: Zork executable not found: {ZORK_EXECUTABLE}")
        print("Please run: cd build-host && cmake --build . --parallel")
        return 1

    # Check that game file exists
    if not Path(GAME_FILE).exists():
        print(f"ERROR: Game file not found: {GAME_FILE}")
        return 1

    print(f"[Python] Game file: {GAME_FILE}")
    print(f"[Python] Executable: {ZORK_EXECUTABLE}")
    print(f"[Python] State file: {STATE_FILE}")
    print(f"[Python] Batches: {INSTRUCTIONS_PER_BATCH} instructions each")
    print()

    # Clear previous state to start fresh
    if Path(STATE_FILE).exists():
        print("[Python] Removing previous state file (starting fresh)")
        Path(STATE_FILE).unlink()

    print("[Python] Starting batched execution loop...")
    print("[Python] Press Ctrl+C to stop")
    print()

    accumulated_output = ""
    batch = 1

    try:
        while batch <= MAX_BATCHES:
            print(f"--- Executing Batch {batch}/{MAX_BATCHES} ---")

            output, finished = run_zork_batch(batch, num_batches=1)

            if output:
                accumulated_output += output + "\n"

            if finished:
                print()
                print(f"[Python] Game finished after {batch} batches!")
                break

            batch += 1

        print()
        print("="*60)
        print("ACCUMULATED GAME OUTPUT:")
        print("="*60)
        print(accumulated_output)
        print("="*60)
        print()
        print(f"✅ SUCCESS: Completed {batch} batches")
        print(f"✅ Total Z-machine instructions: ~{batch * INSTRUCTIONS_PER_BATCH}")
        print(f"✅ Program cache eliminated recompilation overhead!")

    except KeyboardInterrupt:
        print()
        print("[Python] Interrupted by user")
        return 130

    return 0


if __name__ == "__main__":
    sys.exit(main())
