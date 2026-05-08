# SPDX-License-Identifier: Apache-2.0
"""
zork_ttlang.py — Interactive Zork runner using the Python Z-machine.

Stage 1: pure Python, no hardware. Runs with ttlang-sim pyenv.
Stage 2 (future): wrap as ttl.operation for QB2 hardware.

Usage (simulator):
    source ~/code/tt-lang/build/env/activate
    python ttlang/zork_ttlang.py game/zork1.z3
"""
from __future__ import annotations
import sys
from pathlib import Path

# Allow running from tt-zork1 root or from ttlang/ subdir
sys.path.insert(0, str(Path(__file__).parent.parent))

from ttlang.zmachine_v3 import ZMachineV3


INSTRUCTIONS_BATCH = 2000  # instructions per interpret() call; stopped early by waiting_for_input


def _run_until_input(zm: ZMachineV3) -> None:
    """Run the interpreter until it pauses waiting for input (or the game ends).

    Calls interpret() at least once — when input_command is set, the first call
    clears waiting_for_input and resumes execution. Then loops until the next READ.
    """
    zm.interpret(INSTRUCTIONS_BATCH)
    while zm.running and not zm.waiting_for_input:
        zm.interpret(INSTRUCTIONS_BATCH)


def run_interactive(game_path: str) -> None:
    """Run Zork interactively from the command line."""
    game_bytes = Path(game_path).read_bytes()
    zm = ZMachineV3(game_bytes)

    print("[ZMachine V3 — TT-Lang pyenv | quit with 'quit']")
    print(f"[Loaded {len(game_bytes)} bytes from {game_path}]\n")

    # Run startup sequence until first prompt appears
    _run_until_input(zm)
    output = zm.flush_output()
    if output:
        print(output, end="", flush=True)

    # Main interactive loop
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
        _run_until_input(zm)
        output = zm.flush_output()
        if output:
            print(output, end="", flush=True)

        if zm.game_over:
            print("[Game over]")
            break


def main() -> None:
    game_path = sys.argv[1] if len(sys.argv) > 1 else "game/zork1.z3"
    if not Path(game_path).exists():
        print(f"Error: game file not found: {game_path}", file=sys.stderr)
        sys.exit(1)
    run_interactive(game_path)


if __name__ == "__main__":
    main()
