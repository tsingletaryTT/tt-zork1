# remix/input_mapper.py
"""Map freeform player input to a valid Zork command via LLM.

Falls back to returning the original input unchanged if the LLM is
unavailable or returns an empty response. The Z-machine parser handles
any remaining ambiguity — this layer just removes the "I don't understand"
wall for creative inputs.
"""
from __future__ import annotations
from pathlib import Path
from remix.llm import call_ollama
from remix.router import route

_SYSTEM_PROMPT = (
    Path(__file__).parent.parent / "prompts" / "remix_input_system.txt"
).read_text()


def map_input(user_input: str) -> str:
    """Translate freeform user input to a valid Zork command.

    Returns the LLM-mapped command, or the original input if the LLM
    is unavailable or returns nothing useful.
    """
    result = call_ollama(
        system=_SYSTEM_PROMPT,
        user=user_input,
        model=route("map"),
        temperature=0.1,   # low temperature: we want deterministic mapping
    )
    if result and result.strip():
        return result.strip()
    return user_input
