# remix/ascii_artist.py
"""Generate ASCII art illustrations for Zork rooms.

Art is generated once per room and cached — expensive to generate,
free to revisit. Returns empty string if LLM is unavailable, so the
game remains playable without ASCII art.
"""
from __future__ import annotations
from pathlib import Path
from remix.llm import call_llm
from remix.router import route

_SYSTEM_PROMPT = (
    Path(__file__).parent.parent / "prompts" / "ascii_system.txt"
).read_text()

# Prefixes that indicate the model output natural-language preamble instead of art.
# Small models frequently emit these despite being told not to.
_PREAMBLE_PREFIXES = (
    "```", "Here", "Sure", "The ", "This", "Below", "//", "I ", "Let ",
    "OK", "Room", "Art", "Note", "Since", "For ",
)


def _strip_preamble(raw: str) -> str:
    """Drop leading lines that look like natural language or code fences.

    Small models often prepend "Here is the ASCII art:" or open a code
    block before the actual art lines, despite the system prompt saying
    not to. Strip them so _frame() receives clean art lines only.
    """
    lines = raw.strip().splitlines()
    while lines:
        stripped = lines[0].strip()
        if stripped.startswith(_PREAMBLE_PREFIXES) or (
            len(stripped) < 6 and not any(c in stripped for c in "#.|~*/\\-_=^@+")
        ):
            lines.pop(0)
        else:
            break
    # Also strip trailing code-fence lines
    while lines and lines[-1].strip().startswith("```"):
        lines.pop()
    return "\n".join(lines)


def _frame(art: str, room_name: str) -> str:
    """Wrap ASCII art with a room-name header and thin horizontal rules.

    No vertical bars — avoids visual confusion with the TUI panel borders.
    Fixed 48-char width to fill the 52-col context pane with padding.
    """
    lines = art.strip().splitlines()[:6]
    width = 48
    art_width = width - 2  # 2-char indent; content fills the rest
    bar = "─" * width
    # Room name inset into the top bar
    label = f" {room_name} "
    top = label + "─" * max(0, width - len(label))
    # Pad each art line to art_width so every row is flush with the bars
    result = [top] + [f"  {line.rstrip():<{art_width}}" for line in lines] + [bar]
    return "\n".join(result)


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
        raw = call_llm(
            system=_SYSTEM_PROMPT,
            user=prompt,
            model=route("art"),
            temperature=0.75,
        )
        if not raw or not raw.strip():
            self._cache[key] = ""
            return ""

        result = _frame(_strip_preamble(raw), room_name)
        self._cache[key] = result
        return result
