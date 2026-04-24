# remix/router.py
"""Route remix tasks to the appropriate LLM model.

Cheap tasks (input mapping) use a small fast model.
Rich tasks (output remix, ASCII art, postcards) use a larger model.
Override via ZORK_LLM_MODEL env var to force a single model everywhere.
"""
from __future__ import annotations
import os

_OVERRIDE = os.environ.get("ZORK_LLM_MODEL")

_MODELS = {
    # Small model: input mapping — needs speed, not creativity
    "map":      "meta-llama/Llama-3.2-1B-Instruct",
    # Large model: output remix, ASCII art, postcards — needs creativity
    "remix":    "meta-llama/Llama-3.1-8B-Instruct",
    "art":      "meta-llama/Llama-3.1-8B-Instruct",
    "postcard": "meta-llama/Llama-3.1-8B-Instruct",
}


def route(task: str) -> str:
    """Return the model name to use for the given task."""
    if _OVERRIDE:
        return _OVERRIDE
    return _MODELS.get(task, "meta-llama/Llama-3.1-8B-Instruct")
