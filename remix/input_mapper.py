# remix/input_mapper.py
"""Map freeform player input to a valid Zork command via LLM.

Falls back to returning the original input unchanged if the LLM is
unavailable or returns an empty response. The Z-machine parser handles
any remaining ambiguity — this layer just removes the "I don't understand"
wall for creative inputs.

Literal commands (exact Zork syntax) bypass the LLM entirely so that
exact inputs like "go north", "take lamp", "open mailbox" are never
garbled or echoed back as suggestions.  This is particularly important
in auto-play mode where the persona LLM already generates clean commands.
"""
from __future__ import annotations
from pathlib import Path
from remix.llm import call_llm
from remix.router import route

_SYSTEM_PROMPT = (
    Path(__file__).parent.parent / "prompts" / "remix_input_system.txt"
).read_text()

# ---------------------------------------------------------------------------
# Literal-command fast path
# ---------------------------------------------------------------------------

_DIRECTIONS = frozenset({
    "north", "south", "east", "west", "up", "down",
    "northeast", "northwest", "southeast", "southwest",
    "n", "s", "e", "w", "u", "d", "ne", "nw", "se", "sw",
})

_SINGLE_WORD_CMDS = frozenset({
    "look", "l", "inventory", "i", "wait", "z", "again", "g",
    "quit", "score", "verbose", "brief", "superbrief",
    "yes", "no", "diagnose", "version",
})

_ZORK_VERBS = frozenset({
    "take", "get", "drop", "open", "close", "read", "examine", "x",
    "put", "insert", "attack", "kill", "eat", "drink", "wear", "remove",
    "light", "extinguish", "turn", "push", "pull", "climb", "enter",
    "exit", "lock", "unlock", "throw", "give", "show", "wave", "fill",
    "empty", "burn", "go",
})

# Articles and possessives are a reliable signal of prose / freeform input.
# Terse Zork commands never contain them ("take lamp", "open mailbox", etc.).
_PROSE_MARKERS = frozenset({
    "the", "a", "an", "my", "your", "its", "their", "our", "this", "that",
})


def _is_literal_command(text: str) -> bool:
    """Return True when text is already a valid Zork command literal.

    Matches:
    - Single direction words (north, ne, u, …)
    - Single no-argument commands (look, inventory, wait, …)
    - "go <direction>"
    - Any "<known-verb> …" form with no articles/possessives
      (take lamp, open mailbox, attack troll with sword)

    Inputs containing articles ("the", "a", "my", …) are prose and route
    through the LLM regardless of what verb they start with.
    """
    words = text.strip().lower().split()
    if not words:
        return False
    # Prose markers indicate freeform input — always send to LLM.
    if any(w in _PROSE_MARKERS for w in words):
        return False
    first = words[0]
    if len(words) == 1:
        return first in _DIRECTIONS or first in _SINGLE_WORD_CMDS
    # "go north", "go up", etc.
    if first == "go" and words[1] in _DIRECTIONS:
        return True
    # "<verb> ..." — first word is a known Zork verb; trust the whole phrase
    return first in _ZORK_VERBS


def map_input(user_input: str) -> str:
    """Translate freeform user input to a valid Zork command.

    Short-circuits the LLM for inputs that already match Zork command syntax.
    For freeform inputs, calls the LLM and returns its mapping, falling back
    to the original input when the LLM is unavailable or returns nothing.
    """
    clean = user_input.strip()
    if _is_literal_command(clean):
        return clean

    result = call_llm(
        system=_SYSTEM_PROMPT,
        user=clean,
        model=route("map"),
        temperature=0.1,   # low temperature: we want deterministic mapping
    )
    if result and result.strip():
        return result.strip()
    return clean
