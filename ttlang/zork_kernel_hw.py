# SPDX-License-Identifier: Apache-2.0
"""
zork_kernel_hw.py — Hardware TT-Lang kernel for Z-machine execution on QB2.

This is the hardware version of zork_kernel.py, using 'import ttl' instead of
'from sim import ttl'. This allows the kernel to be compiled to C++ via
TTLANG_EMIT_RUNNER=1, generating the DM reader, DM writer, and compute kernels
that run on QB2 RISC-V cores and Tensix compute engine.

Architecture (stub — proves DRAM→DFB→DRAM pipeline works):
    dm_reader  — loads one tile (1024 bytes) of game data from DRAM into L1 DFB
    compute    — waits for game tile, copies it to output DFB (stub)
    dm_writer  — writes output DFB tile back to DRAM output tensor

Tensor layout: TILE_LAYOUT (required by TT-Lang TTNN interop).
Each tile is TILE_SIZE×TILE_SIZE = 32×32 = 1024 float32 elements.
One element per byte (bfloat16 represents 0-255 exactly with 7 mantissa bits).

Buffer sizes (elements = bytes, TILE_LAYOUT padded to tile boundaries):
    GAME_ROWS  = 32,  GAME_COLS  = 2752 → 88064 elements (88 KB, 86 tiles)
    OUT_ROWS   = 32,  OUT_COLS   = 512  → 16384 elements (16 KB, 16 tiles)

The stub DFB transfers one tile (1×1 tile = 32×32 elements = 1024 bytes)
to validate the DRAM→L1→DRAM round-trip. Z-machine header bytes 0-255 survive
inside this first tile.

Usage:
    source ~/code/tt-lang/build/env/activate
    python ttlang/zork_kernel_hw.py game/zork1.z3   # run smoke test on QB2
    # Generate C++ kernels + runner:
    ~/.claude/commands/tools/run-test.sh --hw --emit-runner ttlang/zork_kernel_hw.py
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

import torch
import ttl
import ttnn

# ---------------------------------------------------------------------------
# Tile and buffer geometry
# ---------------------------------------------------------------------------
TILE_SIZE: int = 32         # Tensix tile height = width = 32 elements

# zork1.z3 is 86838 bytes — ceiling to 85 full tiles of 32×32 = 87040 elements.
# Shape (2720, 32): each row holds TILE_SIZE=32 bytes; each tile is 32 rows × 32 cols.
#   • Tile (r_t, 0) = rows [r_t*32:(r_t+1)*32, 0:32] = bytes [r_t*1024 : (r_t+1)*1024].
#   • game[0:32, 0:32] (hardware slice) = bytes 0-1023 sequentially.    ← verified ✓
# 202 bytes of zero-padding fill out the last partial tile; Z-machine never reads past GAME_SIZE.
GAME_SIZE: int  = 86838              # actual zork1.z3 file size
GAME_ROWS: int  = 85 * TILE_SIZE     # 2720  (85 tiles × 32 rows/tile)
GAME_COLS: int  = TILE_SIZE          # 32    (one tile wide; each row = 32 bytes)
GAME_PAD:  int  = GAME_ROWS * GAME_COLS  # 87040 — padded to tile boundary

# Output tensor: 512 rows × 32 cols = 16384 elements = 16 KB = 16 tiles.
# Same scheme: (N_tiles * 32, 32) where each tile holds 1024 output bytes.
OUT_ROWS: int   = 16 * TILE_SIZE     # 512  (= 16 tiles × 32 rows/tile)
OUT_COLS: int   = TILE_SIZE          # 32

# DFB block shape in *tiles* (not elements).
# (1, 1) = one 32×32 tile = 1024 elements = 1024 bytes per block.
DFB_TILES: tuple = (1, 1)


# ---------------------------------------------------------------------------
# TT-Lang operation — hardware version
# ---------------------------------------------------------------------------

@ttl.operation(grid=(1, 1))
def zmachine_kernel(
    game:   ttnn.Tensor,   # shape (GAME_ROWS, GAME_COLS) = (2720, 32) TILE_LAYOUT DRAM
    output: ttnn.Tensor,   # shape (OUT_ROWS,  OUT_COLS)  = (512,  32) TILE_LAYOUT DRAM
) -> None:
    """
    TT-Lang stub kernel: load one tile of game data from DRAM, copy to output.

    Tensor layout (2720, 32):
        • GAME_ROWS = 2720 = 85 × TILE_SIZE; GAME_COLS = 32 = TILE_SIZE
        • Tile (r_t, 0) = rows [r_t*32:(r_t+1)*32, 0:32] = bytes [r_t*1024:(r_t+1)*1024]
        • game[0:32, 0:32] = tile(0, 0) = game bytes 0-1023 sequentially ✓

    Three threads:
        dm_reader — copies game[0:TILE, 0:TILE] (= bytes 0-1023) into game_dfb
        compute   — waits for game tile, stores it to out_dfb  (stub passthrough)
        dm_writer — copies out_dfb tile to output[0:TILE, 0:TILE]

    DFB shape (1, 1) = 1 row × 1 col in tile units = 1 tile = 1024 elements.
    Double-buffered (block_count=2) so DM can prefetch next tile while compute
    processes the current one.

    Full implementation: dm_reader loops over all 85 game tiles; compute runs
    ZMachineV3.interpret(100) on each batch and fills out_dfb with game text.
    """
    # Double-buffered L1 circular buffers — one tile per block = 1024 bytes
    game_dfb = ttl.make_dataflow_buffer_like(game,   shape=DFB_TILES, block_count=2)
    out_dfb  = ttl.make_dataflow_buffer_like(output, shape=DFB_TILES, block_count=2)

    @ttl.compute()
    def compute() -> None:
        """
        Stub: pass game tile through to output.
        Full impl: run Z-machine interpreter, write output text to out_dfb.
        """
        # Wait for dm_reader to fill game_dfb; reserve space in out_dfb
        with game_dfb.wait() as game_blk, out_dfb.reserve() as o_blk:
            o_blk.store(game_blk)  # STUB — replace with ZMachineV3.interpret()

    @ttl.datamovement()
    def dm_reader() -> None:
        """
        DM reader: load first game tile from DRAM into L1 game_dfb.
        game[0:TILE_SIZE, 0:TILE_SIZE] = rows 0-31 × cols 0-31 = tile(0,0) = bytes 0-1023.
        Full impl: loop over all 85 game tiles: for i in range(85): load tile (i, 0).
        """
        with game_dfb.reserve() as g_blk:
            # Read tile(0, 0): rows [0:32], cols [0:32] = game bytes 0-1023
            tx = ttl.copy(game[0:TILE_SIZE, 0:TILE_SIZE], g_blk)
            tx.wait()

    @ttl.datamovement()
    def dm_writer() -> None:
        """
        DM writer: flush output DFB tile back to output DRAM tensor.
        output[0:TILE_SIZE, 0:TILE_SIZE] = tile(0, 0) = output bytes 0-1023.
        """
        with out_dfb.wait() as o_blk:
            # Write tile(0, 0) back to output DRAM tensor
            tx = ttl.copy(o_blk, output[0:TILE_SIZE, 0:TILE_SIZE])
            tx.wait()


# ---------------------------------------------------------------------------
# Host-side runner
# ---------------------------------------------------------------------------

def game_bytes_to_tile_tensor(
    game_bytes: bytes, device: ttnn.Device
) -> ttnn.Tensor:
    """
    Convert raw game bytes to a (GAME_ROWS, GAME_COLS) = (2720, 32) bfloat16
    TILE_LAYOUT DRAM tensor on QB2.

    Shape (2720, 32) where GAME_ROWS = 85*TILE_SIZE, GAME_COLS = TILE_SIZE:
        • Each row holds 32 bytes.
        • Tile (r_t, 0) = rows [r_t*32:(r_t+1)*32, 0:32] = bytes [r_t*1024 : (r_t+1)*1024].
        • kernel slice game[0:32, 0:32] = bytes 0-1023 sequentially.     ← verified ✓

    Padding: 202 zero bytes appended so total = 87040 = 85×1024 (exact tile fit).
    The Z-machine never reads past byte 86838, so padding is safe.
    """
    padded = bytearray(game_bytes[:GAME_SIZE])
    padded += b"\x00" * (GAME_PAD - len(padded))  # 202 bytes zero-padding
    t = (
        torch.frombuffer(bytes(padded), dtype=torch.uint8)
        .clone()
        .float()                          # uint8 → float32 (bfloat16 on device)
        .view(GAME_ROWS, GAME_COLS)       # reshape to (2720, 32) = 85 tiles
        .to(torch.bfloat16)
    )
    return ttnn.from_torch(
        t,
        dtype=ttnn.bfloat16,
        layout=ttnn.TILE_LAYOUT,
        device=device,
        memory_config=ttnn.DRAM_MEMORY_CONFIG,
    )


def run_on_device(game_path: str | Path, device: ttnn.Device) -> bytearray:
    """
    Load game data onto QB2 DRAM, run the kernel, return first 1024 output bytes.

    In the stub, the output contains the first 1024 bytes of the game file
    (one tile), proving the DRAM→DFB→DRAM pipeline works correctly.
    The Z-machine V3 header sits within this first tile (first 64 bytes).
    """
    game_bytes = Path(game_path).read_bytes()
    assert game_bytes[0] == 3, f"Expected V3 game, got version {game_bytes[0]}"

    # Game tensor: (32, 2752) bfloat16 TILE_LAYOUT DRAM
    game_t = game_bytes_to_tile_tensor(game_bytes, device)

    # Output tensor: (512, 32) = (16*TILE_SIZE, TILE_SIZE) bfloat16 TILE_LAYOUT DRAM, zero-init
    # Same layout scheme as game tensor: each row = 32 bytes, tile(0,0) = bytes 0-1023.
    out_torch = torch.zeros(OUT_ROWS, OUT_COLS, dtype=torch.bfloat16)
    output_t = ttnn.from_torch(
        out_torch,
        dtype=ttnn.bfloat16,
        layout=ttnn.TILE_LAYOUT,
        device=device,
        memory_config=ttnn.DRAM_MEMORY_CONFIG,
    )

    print(f"  [QB2] Game tensor:   {game_t.shape}")
    print(f"  [QB2] Output tensor: {output_t.shape}")

    # Execute the TT-Lang kernel on QB2 (RISC-V DM + Tensix compute)
    zmachine_kernel(game_t, output_t)

    # Download and flatten — first TILE_SIZE*TILE_SIZE = 1024 bytes are the payload
    result = ttnn.to_torch(output_t).view(-1)
    tile_bytes = bytearray(int(round(float(v))) & 0xFF for v in result[:TILE_SIZE * TILE_SIZE].tolist())
    return tile_bytes


def _smoke_test(game_path: str) -> None:
    """
    Run the kernel on QB2, check that Z-machine header fields survive the
    DRAM→DFB→DRAM round-trip inside the first 1024-byte tile.

    Checks:
        output[0]         == 3          (Z-machine version byte)
        output[0x06:0x08] == 0x50D5    (initial PC from header bytes 6-7)
        output[0x18:0x1A] == 0x01F0    (abbreviation table address)
    """
    print("[zork_kernel_hw] Hardware smoke test")
    print(f"  game:        {game_path}")
    print(f"  GAME_SIZE:   {GAME_SIZE} bytes  ({GAME_PAD} padded, {GAME_ROWS}x{GAME_COLS} tiles)")
    print(f"  TILE_SIZE:   {TILE_SIZE}x{TILE_SIZE} = {TILE_SIZE*TILE_SIZE} bytes per tile")
    print(f"  DFB shape:   {DFB_TILES} tiles")
    print()

    device = ttnn.open_device(device_id=0)
    try:
        output = run_on_device(game_path, device)
    finally:
        ttnn.close_device(device)

    game_bytes = Path(game_path).read_bytes()
    tile_bytes = TILE_SIZE * TILE_SIZE  # 1024 bytes in one 32×32 tile

    # 1. Z-machine version byte
    assert output[0] == 3, f"Version byte: got {output[0]}, expected 3"
    print(f"  [OK] Version byte: {output[0]}")

    # 2. Initial PC at bytes 6-7 (big-endian) = 0x50D5
    pc = (output[0x06] << 8) | output[0x07]
    assert pc == 0x50D5, f"Initial PC: got 0x{pc:04X}, expected 0x50D5"
    print(f"  [OK] Initial PC: 0x{pc:04X}")

    # 3. Abbreviation table address at bytes 0x18-0x19 = 0x01F0
    abbrev = (output[0x18] << 8) | output[0x19]
    assert abbrev == 0x01F0, f"Abbrev table: got 0x{abbrev:04X}, expected 0x01F0"
    print(f"  [OK] Abbreviation table: 0x{abbrev:04X}")

    # 4. Full tile integrity — all 1024 bytes of the first tile must match game file
    for i in range(tile_bytes):
        assert output[i] == game_bytes[i], (
            f"Byte mismatch at offset 0x{i:04X}: "
            f"got {output[i]}, expected {game_bytes[i]}"
        )
    print(f"  [OK] All {tile_bytes} bytes (first tile) match game file exactly.")
    print()
    print("[zork_kernel_hw] Smoke test PASSED.")
    print()
    print("Next: emit C++ kernels with:")
    print("  ~/.claude/commands/tools/run-test.sh --hw --emit-runner ttlang/zork_kernel_hw.py")


if __name__ == "__main__":
    game = sys.argv[1] if len(sys.argv) > 1 else "game/zork1.z3"
    if not Path(game).exists():
        candidate = Path(__file__).parent.parent / game
        if candidate.exists():
            game = str(candidate)
        else:
            print(f"Error: game file not found: {game}", file=sys.stderr)
            sys.exit(1)
    _smoke_test(game)
