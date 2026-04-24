# remix/llm.py
"""LLM client for the remix layer — targets tt-inference-server by default.

Uses the OpenAI-compatible /v1/chat/completions endpoint. Override the URL
via ZORK_LLM_URL to point at any compatible server (tt-inference-server,
Ollama, etc.). All functions return None on any error — callers must handle
None gracefully so the game remains playable when the LLM is unavailable.
"""
from __future__ import annotations
import os
import requests

TT_INFERENCE_URL = os.environ.get(
    "ZORK_LLM_URL",
    "http://localhost:8000/v1/chat/completions",
)
TIMEOUT = 30  # seconds — tt-inference-server first-token latency can be higher


def call_llm(
    system: str,
    user: str,
    model: str = "meta-llama/Llama-3.1-8B-Instruct",
    temperature: float = 0.7,
) -> str | None:
    """Call tt-inference-server and return the assistant's reply, or None on any error."""
    try:
        resp = requests.post(
            TT_INFERENCE_URL,
            json={
                "model": model,
                "messages": [
                    {"role": "system", "content": system},
                    {"role": "user",   "content": user},
                ],
                "temperature": temperature,
                "stream": False,
            },
            timeout=TIMEOUT,
        )
        if resp.status_code != 200:
            return None
        return resp.json()["choices"][0]["message"]["content"].strip()
    except Exception:
        return None
