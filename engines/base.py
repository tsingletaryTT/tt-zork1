# engines/base.py
from __future__ import annotations
from abc import ABC, abstractmethod


class BaseEngine(ABC):
    """Common interface for all Z-machine backends."""

    def __init__(self, game_path: str) -> None:
        self.game_path = game_path

    @abstractmethod
    def startup(self) -> str:
        """Run the game's initialization sequence. Returns opening text."""
        ...

    @abstractmethod
    def step(self, command: str) -> str:
        """Submit one command, return the game's response."""
        ...

    @abstractmethod
    def close(self) -> None:
        """Release any hardware or file resources."""
        ...

    @property
    @abstractmethod
    def game_over(self) -> bool:
        """True when the game has ended (death or victory)."""
        ...

    @property
    @abstractmethod
    def running(self) -> bool:
        """False when the game has quit cleanly."""
        ...

    def __enter__(self) -> "BaseEngine":
        return self

    def __exit__(self, *_) -> None:
        self.close()
