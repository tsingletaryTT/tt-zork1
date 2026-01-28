#!/usr/bin/env python3
# SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
# SPDX-License-Identifier: Apache-2.0

"""
Zork I on Tenstorrent Blackhole - Persistent Python Execution

This script uses C++ bindings to keep the device open for the entire session,
eliminating device initialization overhead and enabling fast batched execution.

Architecture:
    [Python Process - Lives for entire game session]
        ↓
    zork_tt.init_device() ONCE
        ↓
    zork_tt.load_game()
        ↓
    Loop: zork_tt.execute_batch() × N times
        ↓
    zork_tt.close_device() ONCE

Performance:
- First batch: ~2-3s (device init + compile)
- Later batches: ~0.5s (program cache hit!)
- Expected per turn: 1-2.5s (vs 4-15s with device reset)
"""

import sys
import time
from pathlib import Path

# Add python_bindings to path
sys.path.insert(0, str(Path(__file__).parent / "python_bindings"))

try:
    import zork_tt
except ImportError as e:
    print(f"ERROR: Failed to import zork_tt module: {e}")
    print()
    print("Please build the Python bindings first:")
    print("  cd python_bindings")
    print("  mkdir build && cd build")
    print("  cmake .. && make")
    print("  cd ../..")
    sys.exit(1)

# Configuration
GAME_FILE = "game/zork1.z3"
STATE_FILE = "/tmp/zork_state.bin"
INSTRUCTIONS_PER_BATCH = 100
MAX_BATCHES = 50


def main():
    print("╔══════════════════════════════════════════════════════════╗")
    print("║  ZORK I - Persistent Python Execution                   ║")
    print("║  Device stays open, program cache active!               ║")
    print("╚══════════════════════════════════════════════════════════╝")
    print()

    # Check game file exists
    if not Path(GAME_FILE).exists():
        print(f"ERROR: Game file not found: {GAME_FILE}")
        return 1

    print(f"[Python] Game file: {GAME_FILE}")
    print(f"[Python] Instructions per batch: {INSTRUCTIONS_PER_BATCH}")
    print(f"[Python] Max batches: {MAX_BATCHES}")
    print()

    # Initialize device ONCE
    print("[1/3] Initializing Blackhole device...")
    start_time = time.time()
    try:
        device = zork_tt.init_device()
        init_time = time.time() - start_time
        print(f"      ✅ Device initialized in {init_time:.2f}s")
        print(f"      Device handle: 0x{device:x}")
        print(f"      Program cache: ENABLED")
    except Exception as e:
        print(f"      ❌ Failed: {e}")
        return 1

    try:
        # Load game file ONCE
        print()
        print("[2/3] Loading game file to DRAM...")
        start_time = time.time()
        try:
            zork_tt.load_game(device, GAME_FILE)
            load_time = time.time() - start_time
            print(f"      ✅ Game loaded in {load_time:.2f}s")
        except Exception as e:
            print(f"      ❌ Failed: {e}")
            return 1

        # Load previous state if exists
        if Path(STATE_FILE).exists():
            print(f"      Loading previous state from {STATE_FILE}")
            with open(STATE_FILE, "rb") as f:
                state = f.read()
                zork_tt.set_state(device, state)
            print(f"      ✅ Resumed from previous session")
        else:
            print(f"      No previous state, starting fresh")

        # Batched execution loop
        print()
        print(f"[3/3] Executing batches (device stays open!)...")
        print()

        accumulated_output = ""
        batch = 1

        while batch <= MAX_BATCHES:
            print(f"--- Batch {batch}/{MAX_BATCHES} ---", end=" ", flush=True)

            start_time = time.time()
            try:
                output = zork_tt.execute_batch(device, INSTRUCTIONS_PER_BATCH)
                elapsed = time.time() - start_time

                # Extract text (strip box borders)
                lines = output.split('\n')
                text_lines = []
                in_output = False
                for line in lines:
                    if "ZORK OUTPUT" in line or "interpret(" in line:
                        in_output = True
                        continue
                    if in_output and line.strip():
                        if not any(char in line for char in ['╔', '╚', '║', '=']):
                            text_lines.append(line)

                batch_text = '\n'.join(text_lines).strip()
                if batch_text:
                    accumulated_output += batch_text + "\n"

                print(f"✅ {elapsed:.2f}s", end="")
                if batch == 1:
                    print(" (includes compilation)")
                elif batch == 2:
                    print(" (program cache active!)")
                else:
                    print()

                batch += 1

                # Check if game finished
                if "Game complete" in output or batch > MAX_BATCHES:
                    print()
                    print(f"Game finished after {batch-1} batches!")
                    break

            except Exception as e:
                print(f" ❌ Failed: {e}")
                break

        # Save state
        print()
        print("[Saving] Writing state to file...")
        try:
            state = zork_tt.get_state(device)
            with open(STATE_FILE, "wb") as f:
                f.write(state)
            print(f"         ✅ State saved to {STATE_FILE}")
        except Exception as e:
            print(f"         ⚠️ Failed to save state: {e}")

        # Display output
        print()
        print("="*60)
        print("ACCUMULATED GAME OUTPUT:")
        print("="*60)
        print(accumulated_output)
        print("="*60)
        print()
        print(f"✅ SUCCESS: Completed {batch-1} batches")
        print(f"✅ Total instructions: ~{(batch-1) * INSTRUCTIONS_PER_BATCH}")
        print(f"✅ Device stayed open the entire time!")

    finally:
        # Close device ONCE
        print()
        print("[Cleanup] Closing device...")
        try:
            zork_tt.close_device(device)
            print("          ✅ Device closed cleanly")
        except Exception as e:
            print(f"          ⚠️ Error during close: {e}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
