# remix/ascii_artist.py
"""Generate ASCII art illustrations for Zork rooms.

Art is generated once per room and cached — expensive to generate,
free to revisit. Returns empty string if LLM is unavailable, so the
game remains playable without ASCII art.
"""
from __future__ import annotations
from pathlib import Path
from remix.llm import call_ollama
from remix.router import route

_SYSTEM_PROMPT = (
    Path(__file__).parent.parent / "prompts" / "ascii_system.txt"
).read_text()


def _frame(art: str, room_name: str) -> str:
    """Wrap ASCII art in a box border with room name."""
    lines = art.strip().splitlines()
    # Truncate to 6 lines max
    lines = lines[:6]
    width = max((len(line) for line in lines), default=0)
    width = max(width, len(room_name) + 2, 20)
    bar = "═" * (width + 2)
    framed = [f"╔{bar}╗", f"║ {room_name:<{width}} ║"]
    for line in lines:
        framed.append(f"║ {line:<{width}} ║")
    framed.append(f"╚{bar}╝")
    return "\n".join(framed)


class AsciiArtist:
    """Room ASCII art generator with per-room cache."""

    def __init__(self) -> None:
        self._cache: dict[str, str] = {}

    def get(self, room_name: str, description: str) -> str:
        """Return framed ASCII art for this room, generating if not cached.

        Returns empty string if the LLM is unavailable.
        """
        key = room_name.strip().lower()
        if key in self._cache:
            return self._cache[key]

        prompt = f"Room: {room_name}\nDescription: {description[:200]}"
        raw = call_ollama(
            system=_SYSTEM_PROMPT,
            user=prompt,
            model=route("art"),
            temperature=0.9,
        )
        if not raw or not raw.strip():
            self._cache[key] = ""
            return ""

        result = _frame(raw, room_name)
        self._cache[key] = result
        return result
