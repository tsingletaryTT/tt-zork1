"""
test_noc_read_kernel.py — Diagnostic: test DRAM→L1 NOC reads for 87 KB game data.

This isolates whether the NOC read phase (22 concurrent reads of the game binary)
is what's causing the Stage 3 hang, or whether the interpreter is the culprit.

Usage:
    source ~/code/tt-lang/build/env/activate
    cd /home/ttuser/code/tt-zork1
    python ttlang/test_noc_read_kernel.py

Expected output on success:
    NOC_READ_OK header=03 00 XX XX XX XX XX XX

    [PASS] NOC read of 87 KB game data completed — reads are not the hang source.

If this hangs, the issue is in the batched NOC reads. In that case, try adding
intermediate barriers (every 8 reads) in zork_interpreter_l1.cpp.

If this passes but zork_interpreter_l1.cpp still hangs, the issue is in
the Z-machine interpreter or state save/load.
"""
from __future__ import annotations
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))
import torch
import ttnn

KERNEL_PATH = str(Path(__file__).parent.parent / "kernels" / "test_noc_read.cpp")
GAME_PATH   = str(Path(__file__).parent.parent / "game" / "zork1.z3")

GAME_SIZE = 87040  # matches kernel constexpr
OUTPUT_SIZE = 256  # just need a small buffer for the confirmation message

_CORE = ttnn.CoreCoord(0, 0)
_CORE_RANGES = ttnn.CoreRangeSet([ttnn.CoreRange(_CORE, _CORE)])


def run_test(config_name: str, config) -> bool:
    print(f"\n--- Testing NOC read with {config_name} ---")
    device = ttnn.open_device(device_id=0)
    try:
        # Load game binary as uint8 (raw bytes, one element per byte)
        game_bytes = Path(GAME_PATH).read_bytes()
        assert game_bytes[0] == 3, f"Expected Z-machine V3, got {game_bytes[0]}"

        padded = bytearray(game_bytes[:GAME_SIZE])
        padded += b"\x00" * (GAME_SIZE - len(padded))
        t_game = torch.frombuffer(bytes(padded), dtype=torch.uint8).clone()
        game_t = ttnn.from_torch(t_game, dtype=ttnn.uint8, layout=ttnn.ROW_MAJOR_LAYOUT,
                                 device=device, memory_config=ttnn.DRAM_MEMORY_CONFIG)

        # Output buffer: small, bfloat16 (just enough for the confirmation string)
        t_out = torch.zeros(OUTPUT_SIZE, dtype=torch.float32)
        output_t = ttnn.from_torch(t_out, dtype=ttnn.bfloat16, layout=ttnn.ROW_MAJOR_LAYOUT,
                                   device=device, memory_config=ttnn.DRAM_MEMORY_CONFIG)

        game_addr   = game_t.buffer_address()
        output_addr = output_t.buffer_address()
        print(f"  game tensor:   {game_addr:#010x}  ({GAME_SIZE} bytes)")
        print(f"  output tensor: {output_addr:#010x}")

        defines = [
            ("GAME_DRAM_ADDR",   hex(game_addr)),
            ("OUTPUT_DRAM_ADDR", hex(output_addr)),
        ]
        kernel_desc = ttnn.KernelDescriptor(
            kernel_source=KERNEL_PATH,
            source_type=ttnn.KernelDescriptor.SourceType.FILE_PATH,
            core_ranges=_CORE_RANGES,
            compile_time_args=[],
            named_compile_time_args=[],
            defines=defines,
            common_runtime_args=[],
            config=config,
        )
        program = ttnn.ProgramDescriptor(kernels=[kernel_desc], cbs=[], semaphores=[])

        print("  Launching kernel (timeout = 60s)...")
        t0 = time.time()
        ttnn.generic_op([game_t, output_t], program)
        elapsed = time.time() - t0
        print(f"  generic_op elapsed: {elapsed:.3f}s")

        # Read back output: raw bytes via bfloat16 view
        t_bf16 = ttnn.to_torch(output_t).to(torch.bfloat16)
        raw = bytes(t_bf16.view(torch.uint8).numpy())
        end = raw.find(b"\x00")
        if end >= 0:
            raw = raw[:end]
        text = raw.decode("ascii", errors="replace")

        print(f"  output: {text!r}")
        if "NOC_READ_OK" in text:
            print(f"  [PASS] NOC read of {GAME_SIZE} bytes completed in {elapsed:.3f}s!")
            return True
        else:
            print(f"  [FAIL] Output missing 'NOC_READ_OK' — NOC reads may have succeeded")
            print(f"         but output write failed, or game data is wrong.")
            return False
    finally:
        ttnn.close_device(device)


if __name__ == "__main__":
    if not Path(GAME_PATH).exists():
        print(f"Error: game file not found: {GAME_PATH}", file=sys.stderr)
        sys.exit(1)

    print(f"Testing NOC read of {GAME_SIZE}-byte game file from DRAM...")
    print(f"Kernel: {KERNEL_PATH}")

    passed = run_test("ReaderConfigDescriptor (NCRISC)", ttnn.ReaderConfigDescriptor())

    if passed:
        print()
        print("[OK] NOC reads are fine — the Stage 3 hang is in the interpreter or state I/O,")
        print("     NOT in the game data loading phase.")
        print()
        print("Next step: run test_defines_kernel.py then ttlang/zork_risc.py --batches 1")
        sys.exit(0)
    else:
        print()
        print("[FAIL] NOC reads of game data are broken.")
        print("       This is the root cause of the Stage 3 hang.")
        print("       Fix: add intermediate barriers every 8 reads in zork_interpreter_l1.cpp")
        sys.exit(1)
