# engines/sim.py
from __future__ import annotations
from pathlib import Path
from engines.base import BaseEngine
from ttlang.zmachine_v3 import ZMachineV3

INSTRUCTIONS_BATCH = 2000  # instructions per interpret() call; loop runs until waiting_for_input


def _run_until_input(zm: ZMachineV3) -> None:
    """Run the interpreter until it pauses waiting for input (or the game ends).

    Calls interpret() at least once — when input_command is set, the first call
    clears waiting_for_input and resumes execution. Then loops until the next READ.
    Required for V5 games (e.g. Adventure) that need ~16k instructions per turn;
    a fixed budget would drop commands mid-turn.
    """
    zm.interpret(INSTRUCTIONS_BATCH)
    while zm.running and not zm.waiting_for_input:
        zm.interpret(INSTRUCTIONS_BATCH)


class SimEngine(BaseEngine):
    """Pure-Python Z-machine engine. No hardware required. Stage 1 of the demo arc."""

    label = "Stage 1 — Pure Python Z-machine (no hardware)"

    def __init__(self, game_path: str) -> None:
        super().__init__(game_path)
        game_bytes = Path(game_path).read_bytes()
        self._zm = ZMachineV3(game_bytes)

    def startup(self) -> str:
        _run_until_input(self._zm)
        return self._zm.flush_output()

    def step(self, command: str) -> str:
        # Returns empty string if called after game has ended (running=False)
        self._zm.input_command = command
        _run_until_input(self._zm)
        return self._zm.flush_output()

    @property
    def game_over(self) -> bool:
        return self._zm.game_over

    @property
    def running(self) -> bool:
        return self._zm.running

    def close(self) -> None:
        pass
