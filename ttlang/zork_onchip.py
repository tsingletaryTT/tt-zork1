# SPDX-License-Identifier: Apache-2.0
"""
zork_onchip.py — Z-machine on QB2 RISC-V via TT-Lang exported kernels.

This is the "Path C" implementation: use the TT-Lang exported C++ kernels
(ttlang/exported/kernels/*.cpp) directly through ttnn.generic_op, running
the game data transfer pipeline on QB2 RISC-V DM cores.

Current status (stub):
    The dm_reader kernel loads the first 1024-byte tile of zork1.z3 from DRAM
    into L1. The compute kernel copies it to the output CB. The dm_writer kernel
    writes it back to DRAM. This proves the complete DRAM→RISC-V→DRAM pipeline.

    Header fields verified from the on-chip round-trip:
        Version byte: 3 (Z-machine V3)
        Initial PC:   0x50D5
        Abbrev table: 0x01F0

Next steps (to achieve Zork text output on-chip):
    1. Modify dm_reader to loop over all 85 tiles and push them to game_dfb.
    2. Replace compute stub (copy_tile) with the ZMachineV3.interpret(100) logic.
    3. Have dm_writer drain output tiles from out_dfb to DRAM output tensor.
    4. Host reads output bytes and decodes ASCII game text.

This file is the entry point. Kernel C++ source is in ttlang/exported/kernels/.

Usage:
    source ~/code/tt-lang/build/env/activate
    python ttlang/zork_onchip.py game/zork1.z3

    # Or via run-test.sh:
    ~/.claude/commands/tools/run-test.sh --hw ttlang/zork_onchip.py

Architecture diagram:
    Host (x86)
      ↓ ttnn.from_torch → DRAM
    QB2 DRAM: game_tensor (2720×32 bfloat16 TILE_LAYOUT)
      ↓ noc_async_read_tile (dm_reader / RISC-V DM0)
    QB2 L1:   game_dfb (circular buffer, double-buffered, 1 tile = 1024 bytes)
      ↓ copy_tile (compute / Tensix)
    QB2 L1:   out_dfb (circular buffer, double-buffered, 1 tile = 1024 bytes)
      ↓ noc_async_write_tile (dm_writer / RISC-V DM1)
    QB2 DRAM: output_tensor (512×32 bfloat16 TILE_LAYOUT)
      ↓ ttnn.to_torch ← DRAM
    Host (x86): reads Z-machine output text

See also:
    ttlang/zork_kernel_hw.py — TT-Lang source that generated the C++ kernels
    ttlang/exported/kernels/reader.cpp  — RISC-V DM0 kernel (DRAM→L1)
    ttlang/exported/kernels/compute.cpp — Tensix compute kernel (L1→L1)
    ttlang/exported/kernels/writer.cpp  — RISC-V DM1 kernel (L1→DRAM)
    ttlang/exported/run_kernel.py       — standalone runner (same logic as here)
    ttlang/zmachine_v3.py               — Python Z-machine interpreter (host-side)
    ttlang/zork_device.py               — Python interpreter with DRAM-backed game data
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

# Import the exported standalone runner — it has all the kernel/tensor logic.
# zork_onchip.py is a thin wrapper that adds a friendlier CLI and summary output.
from ttlang.exported.run_kernel import (
    load_game,
    make_output,
    read_tile,
    run,
    TILE_SIZE,
    GAME_SIZE,
    GAME_PAD,
    GAME_ROWS,
    GAME_COLS,
    OUT_ROWS,
    OUT_COLS,
    KERNELS_DIR,
    KERNEL_PATHS,
)

import ttnn


def run_zork_onchip(game_path: str | Path) -> bytearray:
    """
    Load zork1.z3 into QB2 DRAM, execute the on-chip kernel pipeline,
    and return the output bytes from DRAM.

    Returns:
        bytearray of TILE_SIZE*TILE_SIZE = 1024 bytes.
        In the stub: bytes 0-1023 of the game file (Z-machine header + data).
        After full implementation: Z-machine output text for the first turn.
    """
    device = ttnn.open_device(device_id=0)
    try:
        game_t   = load_game(game_path, device)
        output_t = make_output(device)
        run([game_t, output_t], device)
        return read_tile(output_t)
    finally:
        ttnn.close_device(device)


def _main(game_path: str) -> None:
    """Run the on-chip pipeline and display results."""
    game_bytes = Path(game_path).read_bytes()

    print("=" * 60)
    print("  ZORK ON QB2 RISC-V — TT-Lang Exported Kernels")
    print("=" * 60)
    print()
    print(f"  Game file:    {game_path} ({len(game_bytes)} bytes)")
    print(f"  Game tensor:  ({GAME_ROWS}, {GAME_COLS}) bfloat16 TILE_LAYOUT DRAM")
    print(f"  Out tensor:   ({OUT_ROWS}, {OUT_COLS}) bfloat16 TILE_LAYOUT DRAM")
    print(f"  Tile size:    {TILE_SIZE}×{TILE_SIZE} = {TILE_SIZE*TILE_SIZE} bytes/tile")
    print()
    print("  Kernels:")
    for path, thread_type in KERNEL_PATHS:
        print(f"    [{thread_type:8s}] {Path(path).name}")
    print()
    print("  Executing on QB2...")

    output = run_zork_onchip(game_path)

    print()
    print("  On-chip execution complete.")
    print()

    # Verify Z-machine header fields
    tile_bytes = TILE_SIZE * TILE_SIZE  # 1024
    ok = True

    v = output[0]
    print(f"  Version byte:      {v}  {'[OK]' if v == 3 else '[FAIL]'}")
    ok = ok and (v == 3)

    pc = (output[6] << 8) | output[7]
    print(f"  Initial PC:        0x{pc:04X}  {'[OK]' if pc == 0x50D5 else '[FAIL]'}")
    ok = ok and (pc == 0x50D5)

    abbrev = (output[0x18] << 8) | output[0x19]
    print(f"  Abbrev table:      0x{abbrev:04X}  {'[OK]' if abbrev == 0x01F0 else '[FAIL]'}")
    ok = ok and (abbrev == 0x01F0)

    mismatches = sum(1 for i in range(tile_bytes) if output[i] != game_bytes[i])
    print(f"  Tile integrity:    {tile_bytes - mismatches}/{tile_bytes} bytes match  {'[OK]' if mismatches == 0 else '[FAIL]'}")
    ok = ok and (mismatches == 0)

    print()
    if ok:
        print("  RESULT: PASSED — Z-machine game data lives on QB2 RISC-V!")
        print()
        print("  Z-machine header decoded from on-chip output:")
        print(f"    Z-machine version: {output[0]} (V{output[0]})")
        print(f"    Initial PC:        0x{pc:04X}")
        print(f"    Abbrev table:      0x{abbrev:04X}")
        print()
        print("  Stub currently copies game tile 0 (bytes 0-1023) to output.")
        print("  Next step: replace compute stub with ZMachineV3.interpret(100)")
        print("  to produce actual Zork game text on-chip.")
    else:
        print("  RESULT: FAILED — see mismatch details above.")
        sys.exit(1)

    print("=" * 60)


if __name__ == "__main__":
    game = sys.argv[1] if len(sys.argv) > 1 else "game/zork1.z3"
    if not Path(game).exists():
        candidate = Path(__file__).parent.parent / game
        if candidate.exists():
            game = str(candidate)
        else:
            print(f"Error: game file not found: {game}", file=sys.stderr)
            sys.exit(1)
    _main(game)
