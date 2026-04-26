# remix/mode.py
"""RemixLayer — orchestrates input mapping, output remixing, ASCII art, and postcards.

This is the single object play.py creates when --remix is passed. It holds all
remix state (active flag, ASCII art cache, postcard collection) and exposes the
process() method that play.py calls each turn.

Architecture:

    User input
        │
        ▼
    map_input()          — translate freeform English → Zork command (input_mapper.py)
        │
        ▼
    _looks_like_room_entry()
        │  yes → AsciiArtist.get()   — framed ASCII art for this room  (ascii_artist.py)
        │  no  → (skip art)
        │
        ▼
    remix_output()       — rewrite Z-machine text in user's voice    (output_remixer.py)
        │
        ▼
    NarrativeEnhancer.record()   — save memorable moments as postcards
        │
        ▼
    Final display text

On game end, on_game_end() triggers NarrativeEnhancer.display() which prints all
collected postcards to the terminal.
"""
from remix.input_mapper import map_input
from remix.output_remixer import remix_output
from remix.ascii_artist import AsciiArtist
from remix.narrative_enhancer import NarrativeEnhancer

# Heuristics to detect a room-entry response (the Z-machine sends the full
# room description each time you enter a new location).
_ROOM_MARKERS = ("you are", "you're", "this is", "it is", "you can see")


def _looks_like_room_entry(text: str) -> bool:
    """Return True if the Z-machine response looks like entering a new room.

    Room responses typically start with a location description ("You are
    standing…") and span at least two lines. We use a lightweight heuristic
    rather than parsing Z-machine state so this module has no dependency on
    the interpreter internals.
    """
    stripped = text.strip().lower()
    return (
        any(stripped.startswith(m) for m in _ROOM_MARKERS)
        or ("\n" in stripped and len(stripped.splitlines()) >= 2)
    )


def _extract_room_name(text: str) -> str:
    """Best-effort extraction of a room name from Z-machine output.

    The first non-empty line of a room description is almost always the
    room name in Zork ("West of House", "North of House", etc.).
    """
    for line in text.splitlines():
        if line.strip():
            return line.strip()
    return "Unknown Location"


class RemixLayer:
    """Wraps a Z-machine engine's output with LLM-powered remix features.

    Attributes:
        active: When False, process() returns the raw Z-machine response
                unchanged. Toggled by /remix and /classic meta-commands in
                play.py.

    Instantiation is cheap — LLM calls happen lazily inside process() and
    on_game_end().
    """

    def __init__(self) -> None:
        # Public flag: play.py sets this via /classic and /remix commands.
        self.active: bool = True

        # ASCII art generator with per-room cache.
        self._artist = AsciiArtist()

        # Postcard collector; holds state across the whole game session.
        self._enhancer = NarrativeEnhancer()

        # Track whether we have seen the first room yet (for arrival postcard).
        self._first_room: bool = True

    def process(self, user_input: str, zork_response: str) -> str:
        """Process one game turn through the full remix pipeline.

        Args:
            user_input:    The raw text the player typed (may be freeform).
            zork_response: The raw text the Z-machine produced.

        Returns:
            The remixed response string to display to the player. If the
            remix layer is inactive (self.active is False), returns
            zork_response unchanged.

        Pipeline:
            1. map_input()     — translate user_input to a valid Zork command
                                 (used to give remix_output context about intent)
            2. ASCII art       — prepended on room-entry responses, cached
            3. moment tracking — record arrival / death / victory for postcards
            4. remix_output()  — rewrite zork_response with user's voice flavour
        """
        if not self.active:
            return zork_response

        # Step 1: translate input (for remixer context; the engine already ran
        # with the original command — this mapping is purely for voice matching).
        mapped_cmd = map_input(user_input)

        parts: list[str] = []

        # Step 2 & 3: ASCII art and arrival postcard on room entry.
        if _looks_like_room_entry(zork_response):
            room_name = _extract_room_name(zork_response)
            art = self._artist.get(room_name, zork_response)
            if art:
                parts.append(art + "\n")
            if self._first_room:
                self._enhancer.record(
                    room_name,
                    "arrival",
                    f"First location: {room_name}",
                )
                self._first_room = False

        # Detect death/victory for milestone postcards.
        low = zork_response.lower()
        if "you have died" in low or "**** you" in low:
            room_name = _extract_room_name(zork_response)
            self._enhancer.record(room_name, "death", zork_response[:200])
        if "congratulations" in low or "you have won" in low:
            self._enhancer.record("Victory", "victory", zork_response[:200])

        # Step 4: rewrite the Z-machine response to match the player's voice.
        remixed = remix_output(mapped_cmd, zork_response)
        parts.append(remixed)

        return "".join(parts)

    def render_postcards(self) -> str:
        """Return postcards as a formatted string, or '' if none collected.

        Used by the TUI to post postcards as GameText rather than printing
        to stdout behind the Textual screen.
        """
        from remix.narrative_enhancer import _render_postcards
        return _render_postcards(self._enhancer.get_postcards())

    def on_game_end(self) -> None:
        """Print postcards to stdout (CLI / terminal mode).

        The TUI calls render_postcards() and posts the result as GameText
        instead of calling this method, so output appears in the game pane.
        """
        rendered = self.render_postcards()
        if rendered:
            print("\n" + rendered)
