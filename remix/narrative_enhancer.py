# remix/narrative_enhancer.py
"""Narrative postcard generator — ported from feature/multi-llm-integration.

Collects memorable moments during gameplay and generates vivid postcard-style
descriptions at game end. One postcard per moment type (arrival, treasure,
death, victory) to keep the collection concise.
"""
from __future__ import annotations
from pathlib import Path
from remix.llm import call_ollama
from remix.router import route

_SYSTEM_PROMPT = (
    Path(__file__).parent.parent / "prompts" / "postcard_system.txt"
).read_text()

_POSTCARD_TYPES = {"arrival", "treasure", "death", "victory"}

# Display width for postcard boxes
_WIDTH = 44


def _render_postcards(cards: list[dict]) -> str:
    """Render all postcards as a framed display block."""
    if not cards:
        return ""
    bar = "═" * (_WIDTH + 2)
    lines = [f"╔{bar}╗", f"║{'  POSTCARDS FROM YOUR JOURNEY':<{_WIDTH + 2}}║",
             f"╠{bar}╣"]
    for card in cards:
        loc = card["location"]
        text = card["text"]
        lines.append(f"║ {loc:<{_WIDTH}} ║")
        # Word-wrap text to _WIDTH chars; hard-wrap oversized words
        words = text.split()
        line_buf = ""
        for word in words:
            candidate = f"{line_buf} {word}".strip() if line_buf else word
            if len(candidate) > _WIDTH:
                if line_buf:
                    lines.append(f"║ {line_buf:<{_WIDTH}} ║")
                # Hard-wrap oversized words
                while len(word) > _WIDTH:
                    lines.append(f"║ {word[:_WIDTH]:<{_WIDTH}} ║")
                    word = word[_WIDTH:]
                line_buf = word
            else:
                line_buf = candidate
        if line_buf:
            lines.append(f"║ {line_buf:<{_WIDTH}} ║")
        lines.append(f"║ {'':<{_WIDTH}} ║")
    lines.append(f"╚{bar}╝")
    return "\n".join(lines)


class NarrativeEnhancer:
    """Collects game moments and generates postcards at game end."""

    def __init__(self) -> None:
        self._postcards: list[dict] = []
        self._seen_types: set[str] = set()

    def record(self, location: str, moment_type: str, context: str) -> None:
        """Record a memorable moment. One postcard per moment_type."""
        if moment_type not in _POSTCARD_TYPES or moment_type in self._seen_types:
            return
        prompt = (
            f"LOCATION: {location}\nMOMENT: {moment_type}\nCONTEXT: {context}"
        )
        try:
            text = call_ollama(
                system=_SYSTEM_PROMPT,
                user=prompt,
                model=route("postcard"),
                temperature=0.85,
            )
        except Exception:
            return
        if text and text.strip():
            self._postcards.append(
                {"location": location, "moment_type": moment_type, "text": text.strip()}
            )
            self._seen_types.add(moment_type)

    def get_postcards(self) -> list[dict]:
        """Return a copy of the collected postcards list."""
        return list(self._postcards)

    def display(self) -> None:
        """Print all postcards to terminal."""
        rendered = _render_postcards(self._postcards)
        if rendered:
            print("\n" + rendered)
