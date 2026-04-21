# SPDX-License-Identifier: Apache-2.0
"""
run_kernel.py — Standalone TT-Metal runner for the Z-machine kernel.

This is the exported version of ttlang/zork_kernel_hw.py, using
ttnn.generic_op with the TT-Lang-compiled C++ kernels directly.
This program is independent of ttlang and can be run with plain Python.

The three C++ kernels (in kernels/):
    reader.cpp  — dm_reader: DRAM → L1 via noc_async_read_tile
    compute.cpp — compute:   L1 tile copy using copy_tile
    writer.cpp  — dm_writer: L1 → DRAM via noc_async_write_tile

Smoke test: load zork1.z3 into QB2 DRAM, run the kernel, verify that
the first tile (1024 bytes) round-trips correctly. Z-machine header
fields in bytes 0-63 are checked explicitly.

Usage:
    source ~/code/tt-lang/build/env/activate
    python ttlang/exported/run_kernel.py [game/zork1.z3]

Or via run-test.sh:
    ~/.claude/commands/tools/run-test.sh --hw ttlang/exported/run_kernel.py
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent))

import torch
import ttnn

# ---------------------------------------------------------------------------
# Kernel and tensor geometry — must match zork_kernel_hw.py
# ---------------------------------------------------------------------------

TILE_SIZE: int = 32          # Tensix tile = 32×32 elements

# Game tensor shape: (2720, 32) = (85*TILE, TILE)
#   Each row = 32 bytes; tile(r_t, 0) = bytes [r_t*1024 : (r_t+1)*1024].
#   kernel slice game[0:32, 0:32] = tile(0, 0) = bytes 0-1023. ← verified ✓
GAME_SIZE: int  = 86838              # actual zork1.z3 file size
GAME_ROWS: int  = 85 * TILE_SIZE     # 2720
GAME_COLS: int  = TILE_SIZE          # 32
GAME_PAD:  int  = GAME_ROWS * GAME_COLS  # 87040 (202 bytes zero-padding)

# Output tensor shape: (512, 32) — same layout scheme, 16 tiles
OUT_ROWS: int   = 16 * TILE_SIZE     # 512
OUT_COLS: int   = TILE_SIZE          # 32

# Kernel parameters (match CB_CONFIGS from runner_generated.py)
GRID_COLS: int  = 1
GRID_ROWS: int  = 1
NUM_TENSORS: int = 2

# Kernel source paths — relative to this file's directory
_HERE = Path(__file__).parent
KERNELS_DIR = _HERE / "kernels"

# CB configs: ((tile_shape), block_count, dtype, page_size_bytes, total_size_bytes)
# page_size = 2048 = one bfloat16 tile (32*32*2 bytes); total = 2 blocks × 2048 = 4096
CB_CONFIGS = [
    ((1, 1), 2, ttnn.bfloat16, 2048, 4096),  # CB 0 — game_dfb
    ((1, 1), 2, ttnn.bfloat16, 2048, 4096),  # CB 1 — out_dfb
]

# Kernel paths and their thread types (order matters: compute, reader, writer)
KERNEL_PATHS = [
    (str(KERNELS_DIR / "compute.cpp"), "compute"),
    (str(KERNELS_DIR / "reader.cpp"),  "noc"),      # dm_reader (NOC 0)
    (str(KERNELS_DIR / "writer.cpp"),  "noc"),      # dm_writer (NOC 1)
]

# Tensor indices: which tensors each kernel thread accesses via runtime args
# [] = compute (accesses CBs only, no direct tensor args)
# [0] = reader gets game tensor buffer address
# [1] = writer gets output tensor buffer address
KERNEL_TENSOR_INDICES = [
    [],   # compute
    [0],  # reader (game tensor)
    [1],  # writer (output tensor)
]


# ---------------------------------------------------------------------------
# Core runner — builds ttnn.generic_op program and executes
# ---------------------------------------------------------------------------

def run(tensors: list, device=None) -> None:
    """
    Run the Z-machine kernel on QB2 using ttnn.generic_op.

    Args:
        tensors: [game_tensor, output_tensor] — both must be bfloat16 TILE_LAYOUT
                 DRAM tensors on the same device.
        device:  ttnn.Device (inferred from tensors[0] if None).

    After this call, output_tensor contains the first tile (1024 bytes) of
    game data in its first tile slot [0:32, 0:32] (stub behaviour).
    """
    assert len(tensors) == NUM_TENSORS, f"Expected {NUM_TENSORS} tensors, got {len(tensors)}"

    if device is None:
        device = tensors[0].device()

    # Single core range: core (0,0) only (grid = 1×1)
    core_ranges = ttnn.CoreRangeSet([ttnn.CoreRange(
        ttnn.CoreCoord(0, 0),
        ttnn.CoreCoord(GRID_COLS - 1, GRID_ROWS - 1)
    )])

    # TensorAccessorArgs encode DRAM/L1 address, stride, and access pattern.
    # The reader kernel uses tensor 0's args; the writer uses tensor 1's args.
    tensor_accessor_args = []
    for tensor in tensors:
        tensor_accessor_args.extend(ttnn.TensorAccessorArgs(tensor).get_compile_time_args())

    # Circular buffer descriptors — one per DFB
    cb_descriptors = []
    for i, (shape, block_count, dtype, page_size, total_size) in enumerate(CB_CONFIGS):
        cb_format = ttnn.CBFormatDescriptor(
            buffer_index=i,
            data_format=dtype,
            page_size=page_size,
        )
        cb_desc = ttnn.CBDescriptor(
            total_size=total_size,
            core_ranges=core_ranges,
            format_descriptors=[cb_format],
        )
        cb_descriptors.append(cb_desc)

    cb_indices = list(range(len(CB_CONFIGS)))  # [0, 1]

    # Kernel descriptors: one per thread (compute, reader, writer)
    kernel_descriptors = []
    noc_idx = 0

    for kernel_idx, (kernel_path, thread_type) in enumerate(KERNEL_PATHS):
        tensor_indices = KERNEL_TENSOR_INDICES[kernel_idx]
        # runtime args = buffer addresses for the tensors this thread accesses
        common_runtime_args = [tensors[idx].buffer_address() for idx in tensor_indices]

        if thread_type == "compute":
            # Compute kernel: compile-time args = CB indices (for cb_wait_front etc.)
            compile_time_args = cb_indices
            config = ttnn.ComputeConfigDescriptor()
        else:
            # DM kernel: compile-time args = CB indices + tensor accessor args
            compile_time_args = cb_indices + tensor_accessor_args
            if noc_idx == 0:
                config = ttnn.ReaderConfigDescriptor()   # NOC 0 — reader
            else:
                config = ttnn.WriterConfigDescriptor()   # NOC 1 — writer
            noc_idx += 1

        kernel_desc = ttnn.KernelDescriptor(
            kernel_source=kernel_path,
            core_ranges=core_ranges,
            compile_time_args=compile_time_args,
            common_runtime_args=common_runtime_args,
            config=config,
        )
        kernel_descriptors.append(kernel_desc)

    # Build and execute the program
    program = ttnn.ProgramDescriptor(
        kernels=kernel_descriptors,
        cbs=cb_descriptors,
        semaphores=[],
    )

    ttnn.generic_op(list(tensors), program)


# ---------------------------------------------------------------------------
# Host-side tensor helpers
# ---------------------------------------------------------------------------

def load_game(game_path: str | Path, device: ttnn.Device) -> ttnn.Tensor:
    """
    Load zork1.z3 into a (2720, 32) bfloat16 TILE_LAYOUT DRAM tensor.

    Layout: each row = 32 bytes. tile(r_t, 0) = bytes [r_t*1024:(r_t+1)*1024].
    The first tile game[0:32, 0:32] contains bytes 0-1023 (Z-machine header).
    """
    game_bytes = Path(game_path).read_bytes()
    assert game_bytes[0] == 3, f"Expected Z-machine V3 file, got version {game_bytes[0]}"

    padded = bytearray(game_bytes[:GAME_SIZE])
    padded += b"\x00" * (GAME_PAD - len(padded))  # zero-pad to 87040

    t = (
        torch.frombuffer(bytes(padded), dtype=torch.uint8)
        .clone()
        .float()
        .view(GAME_ROWS, GAME_COLS)      # (2720, 32)
        .to(torch.bfloat16)
    )
    return ttnn.from_torch(
        t,
        dtype=ttnn.bfloat16,
        layout=ttnn.TILE_LAYOUT,
        device=device,
        memory_config=ttnn.DRAM_MEMORY_CONFIG,
    )


def make_output(device: ttnn.Device) -> ttnn.Tensor:
    """Allocate a (512, 32) zero-filled bfloat16 TILE_LAYOUT DRAM output tensor."""
    t = torch.zeros(OUT_ROWS, OUT_COLS, dtype=torch.bfloat16)
    return ttnn.from_torch(
        t,
        dtype=ttnn.bfloat16,
        layout=ttnn.TILE_LAYOUT,
        device=device,
        memory_config=ttnn.DRAM_MEMORY_CONFIG,
    )


def read_tile(output_t: ttnn.Tensor) -> bytearray:
    """
    Extract the first 1024 bytes from the output tensor (tile(0, 0)).
    Returns a bytearray of 1024 bytes.
    """
    result = ttnn.to_torch(output_t).view(-1)
    return bytearray(int(round(float(v))) & 0xFF for v in result[:TILE_SIZE * TILE_SIZE].tolist())


# ---------------------------------------------------------------------------
# Smoke test
# ---------------------------------------------------------------------------

def _smoke_test(game_path: str) -> None:
    """
    Open QB2, run the kernel, verify that Z-machine header bytes survive the
    DRAM → CB → DRAM round-trip.

    Checks:
        output[0]              == 3       (Z-machine version byte)
        output[6..7] big-endian == 0x50D5 (initial program counter)
        output[0x18..0x19]     == 0x01F0  (abbreviation table address)
        output[0..1023]        == game[0..1023] (full tile integrity)
    """
    print("[run_kernel] Standalone kernel smoke test")
    print(f"  game:          {game_path}")
    print(f"  GAME_SIZE:     {GAME_SIZE} bytes  ({GAME_PAD} padded)")
    print(f"  Game tensor:   ({GAME_ROWS}, {GAME_COLS}) = 85 tiles")
    print(f"  Output tensor: ({OUT_ROWS}, {OUT_COLS}) = 16 tiles")
    print(f"  Kernel dir:    {KERNELS_DIR}")
    print()

    # Verify all kernel files exist before touching hardware
    for path, _ in KERNEL_PATHS:
        assert Path(path).exists(), f"Kernel not found: {path}"
        print(f"  [OK] Found kernel: {Path(path).name}")
    print()

    game_bytes = Path(game_path).read_bytes()

    device = ttnn.open_device(device_id=0)
    try:
        game_t   = load_game(game_path, device)
        output_t = make_output(device)

        print(f"  [QB2] Game tensor:   {game_t.shape}")
        print(f"  [QB2] Output tensor: {output_t.shape}")

        run([game_t, output_t], device)

        output = read_tile(output_t)
    finally:
        ttnn.close_device(device)

    tile_bytes = TILE_SIZE * TILE_SIZE  # 1024

    # 1. Z-machine version byte
    assert output[0] == 3, f"Version byte: got {output[0]}, expected 3"
    print(f"  [OK] Version byte: {output[0]}")

    # 2. Initial PC (bytes 6-7 big-endian) = 0x50D5
    pc = (output[6] << 8) | output[7]
    assert pc == 0x50D5, f"Initial PC: got 0x{pc:04X}, expected 0x50D5"
    print(f"  [OK] Initial PC: 0x{pc:04X}")

    # 3. Abbreviation table address (bytes 0x18-0x19) = 0x01F0
    abbrev = (output[0x18] << 8) | output[0x19]
    assert abbrev == 0x01F0, f"Abbrev table: got 0x{abbrev:04X}, expected 0x01F0"
    print(f"  [OK] Abbreviation table: 0x{abbrev:04X}")

    # 4. Full tile integrity — all 1024 bytes must match game file
    for i in range(tile_bytes):
        assert output[i] == game_bytes[i], (
            f"Byte mismatch at 0x{i:04X}: got {output[i]}, expected {game_bytes[i]}"
        )
    print(f"  [OK] All {tile_bytes} bytes match game file exactly.")
    print()
    print("[run_kernel] Smoke test PASSED.")
    print()
    print("The exported C++ kernels run correctly on QB2 hardware.")
    print("Next: augment dm_reader to loop over all 85 game tiles and")
    print("      replace compute stub with Z-machine interpreter logic.")


if __name__ == "__main__":
    game = sys.argv[1] if len(sys.argv) > 1 else "game/zork1.z3"
    if not Path(game).exists():
        candidate = Path(__file__).parent.parent.parent / game
        if candidate.exists():
            game = str(candidate)
        else:
            print(f"Error: game file not found: {game}", file=sys.stderr)
            sys.exit(1)
    _smoke_test(game)
