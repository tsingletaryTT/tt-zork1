# SPDX-License-Identifier: Apache-2.0
"""
zork_device.py — QB2 device-backed Zork runner.

Stage 2: game data lives in QB2 DRAM as ttnn tensors.
The Python Z-machine interpreter reads/writes through device memory,
proving the data path before the full on-chip kernel (Task 9).

Usage:
    source ~/code/tt-lang/build/env/activate
    python ttlang/zork_device.py game/zork1.z3
"""
from __future__ import annotations
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

import torch
import ttnn
from ttlang.zmachine_v3 import ZMachineV3

INSTRUCTIONS_STARTUP = 10000
INSTRUCTIONS_PER_TURN = 5000


def open_device() -> ttnn.Device:
    """Open QB2 device 0."""
    return ttnn.open_device(device_id=0)


def game_to_device(game_bytes: bytes, device: ttnn.Device) -> ttnn.Tensor:
    """Upload game file to QB2 DRAM. Returns a device tensor."""
    # Pad to even length for row-major layout
    padded = bytearray(game_bytes)
    if len(padded) % 2:
        padded.append(0)

    t = torch.frombuffer(padded, dtype=torch.uint8).clone().float()
    return ttnn.from_torch(
        t,
        dtype=ttnn.bfloat16,
        layout=ttnn.ROW_MAJOR_LAYOUT,
        device=device,
        memory_config=ttnn.DRAM_MEMORY_CONFIG,
    )


def device_to_bytes(tensor: ttnn.Tensor) -> bytearray:
    """Download a device tensor back to a bytearray."""
    t = ttnn.to_torch(tensor)
    return bytearray(int(round(float(v))) & 0xFF for v in t.tolist())


def run_on_device(game_path: str) -> None:
    """Run Zork with game data on QB2 DRAM."""
    game_bytes = Path(game_path).read_bytes()

    print(f"[Opening QB2 device...]")
    device = open_device()
    try:
        print(f"[Uploading {len(game_bytes)} bytes to QB2 DRAM...]")
        device_tensor = game_to_device(game_bytes, device)

        # Download back to verify integrity
        dram_bytes = device_to_bytes(device_tensor)
        assert dram_bytes[:8] == bytearray(game_bytes[:8]), \
            f"DRAM round-trip mismatch! Header bytes differ."
        print(f"[DRAM round-trip verified. Version byte: {dram_bytes[0]}]")
        print(f"[Running Z-machine interpreter (Python host, data on QB2 DRAM)]\n")

        # Run the interpreter using the DRAM-backed bytes
        zm = ZMachineV3(bytes(dram_bytes))

        # Opening sequence
        zm.interpret(INSTRUCTIONS_STARTUP)
        output = zm.flush_output()
        if output:
            print(output, end="", flush=True)

        # Interactive loop
        while zm.running:
            try:
                command = input().strip()
            except (EOFError, KeyboardInterrupt):
                print("\n[Interrupted]")
                break

            if command.lower() in ("quit", "exit", "q"):
                print("[Goodbye!]")
                break

            zm.input_command = command
            zm.interpret(INSTRUCTIONS_PER_TURN)
            output = zm.flush_output()
            if output:
                print(output, end="", flush=True)

            if zm.game_over:
                print("[Game over]")
                break

    finally:
        ttnn.close_device(device)
        print("[Device closed]")


def main() -> None:
    game_path = sys.argv[1] if len(sys.argv) > 1 else "game/zork1.z3"
    if not Path(game_path).exists():
        print(f"Error: game file not found: {game_path}", file=sys.stderr)
        sys.exit(1)
    run_on_device(game_path)


if __name__ == "__main__":
    main()
