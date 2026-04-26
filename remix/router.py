# remix/router.py
"""Route remix tasks to the appropriate LLM model.

Cheap tasks (input mapping, persona command selection) use a small fast model.
Rich tasks (output remix, ASCII art, postcards) use a larger model.
Override via ZORK_LLM_MODEL env var to force a single model everywhere.
The env var is checked at call time so --model applied before the first LLM
call takes effect without requiring a module reload.
"""
import os

_MODELS = {
    # Small model: fast tasks that need accuracy, not creativity
    "map":     "meta-llama/Llama-3.2-1B-Instruct",
    "persona": "meta-llama/Llama-3.2-1B-Instruct",
    # Large model: creative tasks
    "remix":    "meta-llama/Llama-3.1-8B-Instruct",
    "art":      "meta-llama/Llama-3.1-8B-Instruct",
    "postcard": "meta-llama/Llama-3.1-8B-Instruct",
}


def route(task: str) -> str:
    """Return the model name to use for the given task.

    ZORK_LLM_MODEL env var overrides all routing when set. Checked at call
    time so runtime assignment (e.g. from --model flag) takes effect.
    """
    override = os.environ.get("ZORK_LLM_MODEL")
    if override:
        return override
    return _MODELS.get(task, "meta-llama/Llama-3.1-8B-Instruct")
