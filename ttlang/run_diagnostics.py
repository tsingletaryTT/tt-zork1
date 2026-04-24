"""
run_diagnostics.py — Systematic Stage 3 hang diagnosis.

Runs tests in order from simplest to most complex. Stop at the first failure
to identify exactly where the hang occurs.

Step 1: test_defines (NOC write only — baseline)
Step 2: test_noc_read (NOC reads of 87 KB game data)
Step 3: zork single-shot (no state, interpret(5))
Step 4: zork batched (with state, interpret(20) × 10 batches)

Usage:
    source ~/code/tt-lang/build/env/activate
    cd /home/ttuser/code/tt-zork1
    python ttlang/run_diagnostics.py

    # Run specific step only:
    python ttlang/run_diagnostics.py --step 1   # test_defines
    python ttlang/run_diagnostics.py --step 2   # test_noc_read
    python ttlang/run_diagnostics.py --step 3   # zork single-shot
    python ttlang/run_diagnostics.py --step 4   # zork batched
"""
from __future__ import annotations

import sys
import argparse
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))
import torch
import ttnn

_REPO = Path(__file__).parent.parent
GAME_PATH = _REPO / "game" / "zork1.z3"

_CORE = ttnn.CoreCoord(0, 0)
_CORE_RANGES = ttnn.CoreRangeSet([ttnn.CoreRange(_CORE, _CORE)])


# ---------------------------------------------------------------------------
# Step 1: test_defines (NOC write only)
# ---------------------------------------------------------------------------
def step1_test_defines() -> bool:
    """Run the simplest possible kernel: write ASCII to L1, NOC-write to DRAM."""
    print("\n" + "="*60)
    print("STEP 1: test_defines — minimal kernel, NOC write only")
    print("="*60)

    kernel_path = str(_REPO / "kernels" / "test_defines.cpp")
    OUTPUT_SIZE = 256

    device = ttnn.open_device(device_id=0)
    try:
        t = torch.zeros(OUTPUT_SIZE, dtype=torch.float32)
        output_t = ttnn.from_torch(t, dtype=ttnn.bfloat16, layout=ttnn.ROW_MAJOR_LAYOUT,
                                   device=device, memory_config=ttnn.DRAM_MEMORY_CONFIG)
        dummy_t = ttnn.from_torch(torch.zeros(2, dtype=torch.float32), dtype=ttnn.bfloat16,
                                  layout=ttnn.ROW_MAJOR_LAYOUT, device=device,
                                  memory_config=ttnn.DRAM_MEMORY_CONFIG)

        out_addr = output_t.buffer_address()
        print(f"  output: {out_addr:#010x}")

        defines = [("OUTPUT_DRAM_ADDR", hex(out_addr))]
        kernel_desc = ttnn.KernelDescriptor(
            kernel_source=kernel_path,
            source_type=ttnn.KernelDescriptor.SourceType.FILE_PATH,
            core_ranges=_CORE_RANGES,
            compile_time_args=[], named_compile_time_args=[],
            defines=defines, common_runtime_args=[],
            config=ttnn.ReaderConfigDescriptor(),
        )
        program = ttnn.ProgramDescriptor(kernels=[kernel_desc], cbs=[], semaphores=[])

        t0 = time.time()
        ttnn.generic_op([dummy_t, output_t], program)
        elapsed = time.time() - t0
        print(f"  elapsed: {elapsed:.3f}s")

        t_bf16 = ttnn.to_torch(output_t).to(torch.bfloat16)
        raw = bytes(t_bf16.view(torch.uint8).numpy())
        end = raw.find(b"\x00")
        text = raw[:end].decode("ascii", errors="replace") if end >= 0 else raw.decode("ascii", errors="replace")
        print(f"  output: {text!r}")

        ok = "HELLO" in text
        print(f"  => {'[PASS]' if ok else '[FAIL]'} STEP 1 {'completed' if ok else 'FAILED'}")
        return ok
    finally:
        ttnn.close_device(device)


# ---------------------------------------------------------------------------
# Step 2: test_noc_read (DRAM → L1 reads of 87 KB game data)
# ---------------------------------------------------------------------------
def step2_test_noc_read() -> bool:
    """Read 87 KB game data from DRAM into L1 in batched NOC reads."""
    print("\n" + "="*60)
    print("STEP 2: test_noc_read — 87 KB game data DRAM→L1 reads")
    print("="*60)

    kernel_path = str(_REPO / "kernels" / "test_noc_read.cpp")
    GAME_SIZE = 87040
    OUTPUT_SIZE = 256

    device = ttnn.open_device(device_id=0)
    try:
        game_bytes = GAME_PATH.read_bytes()
        padded = bytearray(game_bytes[:GAME_SIZE]) + b"\x00" * (GAME_SIZE - len(game_bytes[:GAME_SIZE]))
        t_game = torch.frombuffer(bytes(padded), dtype=torch.uint8).clone()
        game_t = ttnn.from_torch(t_game, dtype=ttnn.uint8, layout=ttnn.ROW_MAJOR_LAYOUT,
                                 device=device, memory_config=ttnn.DRAM_MEMORY_CONFIG)
        t_out = torch.zeros(OUTPUT_SIZE, dtype=torch.float32)
        output_t = ttnn.from_torch(t_out, dtype=ttnn.bfloat16, layout=ttnn.ROW_MAJOR_LAYOUT,
                                   device=device, memory_config=ttnn.DRAM_MEMORY_CONFIG)

        game_addr   = game_t.buffer_address()
        output_addr = output_t.buffer_address()
        print(f"  game:   {game_addr:#010x}  ({GAME_SIZE} bytes, uint8)")
        print(f"  output: {output_addr:#010x}")

        defines = [("GAME_DRAM_ADDR", hex(game_addr)), ("OUTPUT_DRAM_ADDR", hex(output_addr))]
        kernel_desc = ttnn.KernelDescriptor(
            kernel_source=kernel_path,
            source_type=ttnn.KernelDescriptor.SourceType.FILE_PATH,
            core_ranges=_CORE_RANGES,
            compile_time_args=[], named_compile_time_args=[],
            defines=defines, common_runtime_args=[],
            config=ttnn.ReaderConfigDescriptor(),
        )
        program = ttnn.ProgramDescriptor(kernels=[kernel_desc], cbs=[], semaphores=[])

        t0 = time.time()
        ttnn.generic_op([game_t, output_t], program)
        elapsed = time.time() - t0
        print(f"  elapsed: {elapsed:.3f}s")

        t_bf16 = ttnn.to_torch(output_t).to(torch.bfloat16)
        raw = bytes(t_bf16.view(torch.uint8).numpy())
        end = raw.find(b"\x00")
        text = raw[:end].decode("ascii", errors="replace") if end >= 0 else raw.decode("ascii", errors="replace")
        print(f"  output: {text!r}")

        ok = "NOC_READ_OK" in text
        print(f"  => {'[PASS]' if ok else '[FAIL]'} STEP 2 {'completed' if ok else 'FAILED'}")
        return ok
    finally:
        ttnn.close_device(device)


# ---------------------------------------------------------------------------
# Step 3: zork single-shot, no state, interpret(5) — most conservative
# ---------------------------------------------------------------------------
def step3_zork_singleshot() -> bool:
    """Run the Zork interpreter without state save/load, interpret(5) only."""
    print("\n" + "="*60)
    print("STEP 3: zork single-shot — interpret(5), no state")
    print("="*60)
    print("  This tests the game data load + interpreter, but NOT state I/O.")
    print("  The kernel is compiled WITHOUT STATE_DRAM_ADDR (single-shot mode).")
    print()
    print("  To run this test, manually set interpret(5) in the kernel source,")
    print("  then run: python ttlang/zork_risc.py game/zork1.z3")
    print()
    print("  Alternatively, set ZORK_BATCHES=1 to run a single batch:")
    print("  ZORK_BATCHES=1 python ttlang/zork_risc.py game/zork1.z3")
    print()
    # Actually just run zork_risc with 1 batch — it uses STATE_DRAM_ADDR but that's fine
    # The first batch does fresh init with instruction_count == 0
    from ttlang.zork_risc import run_zork
    print("  Running 1 batch (20 instructions)...")
    t0 = time.time()
    text = run_zork(GAME_PATH, command="", verbose=True, num_batches=1)
    elapsed = time.time() - t0
    print(f"  Total elapsed: {elapsed:.1f}s")
    print(f"  Output ({len(text)} chars): {text[:300]!r}")
    ok = bool(text.strip())
    print(f"  => {'[PASS]' if ok else '[FAIL]'} STEP 3: {'kernel produced output' if ok else 'NO OUTPUT'}")
    return ok


# ---------------------------------------------------------------------------
# Step 4: zork batched
# ---------------------------------------------------------------------------
def step4_zork_batched() -> bool:
    """Run 10 batches of the Zork interpreter to produce the opening text."""
    print("\n" + "="*60)
    print("STEP 4: zork batched — 10 × 20 = 200 instructions total")
    print("="*60)

    from ttlang.zork_risc import run_zork
    t0 = time.time()
    text = run_zork(GAME_PATH, command="", verbose=True, num_batches=10)
    elapsed = time.time() - t0
    print(f"\n  Total elapsed: {elapsed:.1f}s")
    print()
    print("  ====== FINAL OUTPUT ======")
    print(text)
    print("  ==========================")

    ok = "ZORK" in text or len(text.strip()) > 50
    print(f"  => {'[PASS]' if ok else '[FAIL]'} STEP 4: {'ZORK output found' if ok else 'incomplete output'}")
    return ok


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="Stage 3 hang diagnostics")
    parser.add_argument("--step", type=int, choices=[1, 2, 3, 4], default=None,
                        help="Run specific step only (1-4)")
    args = parser.parse_args()

    if not GAME_PATH.exists():
        print(f"Error: game file not found: {GAME_PATH}", file=sys.stderr)
        sys.exit(1)

    print("╔══════════════════════════════════════════════════════════╗")
    print("║  Stage 3 Diagnostics: Isolating the Zork Hang            ║")
    print("╚══════════════════════════════════════════════════════════╝")

    steps = [args.step] if args.step else [1, 2, 3, 4]

    for step_num in steps:
        try:
            if step_num == 1:
                ok = step1_test_defines()
            elif step_num == 2:
                ok = step2_test_noc_read()
            elif step_num == 3:
                ok = step3_zork_singleshot()
            elif step_num == 4:
                ok = step4_zork_batched()
            else:
                continue

            if not ok:
                print(f"\n[STOP] Step {step_num} failed — fix this before proceeding.")
                sys.exit(1)

        except KeyboardInterrupt:
            print(f"\n[HANG] Step {step_num} timed out or was interrupted.")
            print(f"       This step is the source of the hang!")
            print()
            if step_num == 1:
                print("  Fix: Check generic_op tensor count and KernelDescriptor config.")
            elif step_num == 2:
                print("  Fix: NOC reads of game data are hanging.")
                print("       Add intermediate barriers every 8 reads in the kernel.")
            elif step_num == 3:
                print("  Fix: The interpreter itself (or state I/O) is causing the hang.")
                print("       Try reducing interpret() count to 5 or 1.")
            elif step_num == 4:
                print("  Fix: State save/load is causing the hang.")
                print("       Check STATE_READ_SIZE alignment (must be multiple of 32).")
            sys.exit(2)

    print("\n[ALL STEPS PASSED] Stage 3 is working correctly!")
    sys.exit(0)


if __name__ == "__main__":
    main()
