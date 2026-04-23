# remix/output_remixer.py
"""Rewrite Z-machine output to match the player's phrasing and creativity.

The Z-machine response is the source of truth for game facts. This module
changes the voice, not the facts. Falls back to the original response if
the LLM is unavailable.
"""
from __future__ import annotations
from pathlib import Path
from remix.llm import call_ollama
from remix.router import route

_SYSTEM_PROMPT = (
    Path(__file__).parent.parent / "prompts" / "remix_output_system.txt"
).read_text()


def remix_output(user_input: str, zork_response: str) -> str:
    """Rewrite zork_response to match the flavour of user_input.

    Returns the remixed text, or the original zork_response if the LLM
    is unavailable or returns nothing useful.
    """
    user_msg = (
        f"PLAYER: {user_input}\n"
        f"GAME:\n{zork_response}"
    )
    result = call_ollama(
        system=_SYSTEM_PROMPT,
        user=user_msg,
        model=route("remix"),
        temperature=0.8,   # higher temperature for creative voice
    )
    if result and result.strip():
        return result.strip()
    return zork_response
