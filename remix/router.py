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
    "map":      "qwen2.5:1.5b",   # input mapping: cheap, fast
    "remix":    "qwen2.5:7b",     # output remix: richer reasoning
    "art":      "qwen2.5:7b",     # ASCII art: needs spatial reasoning
    "postcard": "qwen2.5:7b",     # narrative postcards: creative
}


def route(task: str) -> str:
    """Return the model name to use for the given task."""
    if _OVERRIDE:
        return _OVERRIDE
    return _MODELS.get(task, "qwen2.5:1.5b")
