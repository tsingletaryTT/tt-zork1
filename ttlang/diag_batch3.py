"""
Diagnostic: does batch 3 hang because of STATE content, or because it's the 3rd invocation?

Test A: 3 batches with normal state persistence (batch 3 uses state from batch 2)
Test B: 3 batches where batch 3 uses a FRESH state (forces fresh init again)

If Test B's batch 3 completes but Test A's hangs: the state content causes the hang.
If both hang: the 3rd kernel invocation itself hangs (hardware/firmware issue).
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

from ttlang.zork_risc import (
    load_game, make_output, make_input, make_state,
    run_interpreter, read_output, KERNEL_PATH
)
import ttnn
import os

GAME_PATH = Path(__file__).parent.parent / "game" / "zork1.z3"

def run_test(label: str, reset_state_before_batch3: bool) -> None:
    print(f"\n=== {label} ===")
    device = ttnn.open_device(device_id=0)
    try:
        game_t   = load_game(GAME_PATH, device)
        output_t = make_output(device)
        input_t  = make_input(device, "")
        state_t  = make_state(device)

        for batch in range(3):
            if reset_state_before_batch3 and batch == 2:
                # Allocate a FRESH state tensor (IC=0 → forces fresh init)
                state_t = make_state(device)
                print(f"  [Batch 3: FRESH state, IC=0]")

            print(f"  Batch {batch+1}/3: launching...", flush=True)
            run_interpreter(game_t, output_t, input_t, device, state_t=state_t)
            text = read_output(output_t)
            n = len(text.strip())
            print(f"  → {n} chars of output", flush=True)
            if text.strip():
                print(f"  → preview: {text.strip()[:100]!r}", flush=True)

        print(f"  ALL 3 BATCHES COMPLETED for {label}!")
    finally:
        ttnn.close_device(device)

if __name__ == "__main__":
    import time

    # Test B: batch 3 gets fresh state
    # Run this first (simpler) to see if 3rd invocation itself hangs
    run_test("TEST B: batch 3 = fresh state (IC=0)", reset_state_before_batch3=True)
    print("\nTest B done. Resetting chips...", flush=True)
    os.system("tt-smi -r 0 1 2 3 >/dev/null 2>&1")
    time.sleep(8)

    # Test A: normal state persistence (batch 3 uses state from batch 2)
    run_test("TEST A: normal state persistence", reset_state_before_batch3=False)
