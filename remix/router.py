# remix/router.py
"""Route remix tasks to the appropriate LLM model.

Cheap tasks (input mapping, persona command selection) use a small fast model.
Rich tasks (output remix, ASCII art, postcards) use a larger model.
Override via ZORK_LLM_MODEL env var to force a single model everywhere.
The env var is checked at call time so --model applied before the first LLM
call takes effect without requiring a module reload.

Autodetection: on the first routing call this module probes GET /v1/models on
the inference server.  When exactly one model is running it is used for all
tasks (the common tt-inference-server deployment pattern).  When zero or
multiple models are detected the task→model table below is used as fallback.
"""
import os
import urllib.parse

_MODELS = {
    # Small model: fast tasks that need accuracy, not creativity
    "map":     "meta-llama/Llama-3.2-1B-Instruct",
    "persona": "meta-llama/Llama-3.2-1B-Instruct",
    # Large model: creative tasks
    "remix":    "meta-llama/Llama-3.1-8B-Instruct",
    "art":      "meta-llama/Llama-3.1-8B-Instruct",
    "postcard": "meta-llama/Llama-3.1-8B-Instruct",
}

# Module-level detection cache — reset on importlib.reload().
_detect_done: bool = False
_detected: str = ""


def _detect_running_model() -> str:
    """Probe /v1/models once and cache the result.

    Returns the single model name when exactly one model is loaded, or "" when
    the server is unreachable, returns an error, or serves multiple models.
    Multiple-model deployments intentionally keep per-task routing so that a
    small + large model pair is used correctly.
    """
    global _detect_done, _detected
    if _detect_done:
        return _detected
    _detect_done = True
    try:
        import requests as _req
        base = os.environ.get("ZORK_LLM_URL", "http://localhost:8000/v1/chat/completions")
        parsed = urllib.parse.urlparse(base)
        models_url = f"{parsed.scheme}://{parsed.netloc}/v1/models"
        resp = _req.get(models_url, timeout=2)
        if resp.status_code == 200:
            ids = [m["id"] for m in resp.json().get("data", [])]
            if len(ids) == 1:
                _detected = ids[0]
    except Exception:
        pass
    return _detected


def detected_model() -> str:
    """Return the autodetected running model name, or '' if unavailable."""
    return _detect_running_model()


def route(task: str) -> str:
    """Return the model name to use for the given task.

    Priority:
    1. ZORK_LLM_MODEL env var — explicit override, wins over everything.
    2. Autodetected single model — when exactly one model is running, use it
       for all tasks regardless of the table below.
    3. Task table (_MODELS) — per-task routing for multi-model deployments.
    4. Default large model — unknown tasks get the creative model.

    The env var is checked at call time so --model applied before the first
    LLM call takes effect without a module reload.
    """
    override = os.environ.get("ZORK_LLM_MODEL")
    if override:
        return override
    auto = detected_model()
    if auto:
        return auto
    return _MODELS.get(task, "meta-llama/Llama-3.1-8B-Instruct")
