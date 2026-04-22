# engines/sim.py
from __future__ import annotations
from pathlib import Path
from engines.base import BaseEngine
from ttlang.zmachine_v3 import ZMachineV3

INSTRUCTIONS_STARTUP = 10000
INSTRUCTIONS_PER_TURN = 5000


class SimEngine(BaseEngine):
    """Pure-Python Z-machine engine. No hardware required. Stage 1 of the demo arc."""

    label = "Stage 1 — Pure Python Z-machine (no hardware)"

    def __init__(self, game_path: str) -> None:
        super().__init__(game_path)
        game_bytes = Path(game_path).read_bytes()
        self._zm = ZMachineV3(game_bytes)

    def startup(self) -> str:
        self._zm.interpret(INSTRUCTIONS_STARTUP)
        return self._zm.flush_output()

    def step(self, command: str) -> str:
        self._zm.input_command = command
        self._zm.interpret(INSTRUCTIONS_PER_TURN)
        return self._zm.flush_output()

    @property
    def game_over(self) -> bool:
        return self._zm.game_over

    @property
    def running(self) -> bool:
        return self._zm.running

    def close(self) -> None:
        pass
