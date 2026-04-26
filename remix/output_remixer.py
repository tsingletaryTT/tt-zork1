# remix/output_remixer.py
"""Rewrite Z-machine output to match the player's phrasing and creativity.

The Z-machine response is the source of truth for game facts. This module
changes the voice, not the facts. Falls back to the original response if
the LLM is unavailable.
"""
from __future__ import annotations
from pathlib import Path
from typing import Callable
from remix.llm import call_llm, call_llm_stream
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
    result = call_llm(
        system=_SYSTEM_PROMPT,
        user=user_msg,
        model=route("remix"),
        temperature=0.8,   # higher temperature for creative voice
    )
    if result and result.strip():
        return result.strip()
    return zork_response


def remix_output_stream(
    user_input: str,
    game_response: str,
    on_token: Callable[[str], None],
) -> str:
    """Stream remix tokens to on_token callback; return full remixed text.

    Falls back to blocking call_llm() if the stream yields nothing. Always
    returns the complete final string so the caller has the result regardless
    of which path was taken.
    """
    user_msg = (
        f"PLAYER: {user_input}\n"
        f"GAME:\n{game_response}"
    )
    tokens: list[str] = []
    for chunk in call_llm_stream(
        system=_SYSTEM_PROMPT,
        user=user_msg,
        model=route("remix"),
        temperature=0.8,
    ):
        on_token(chunk)
        tokens.append(chunk)

    if tokens:
        result = "".join(tokens).strip()
        return result if result else game_response

    # Stream yielded nothing — fall back to blocking call
    result = call_llm(
        system=_SYSTEM_PROMPT,
        user=user_msg,
        model=route("remix"),
        temperature=0.8,
    )
    return result.strip() if result and result.strip() else game_response
