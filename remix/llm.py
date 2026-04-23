# remix/llm.py
"""Thin Ollama HTTP client for the remix layer.

Uses the OpenAI-compatible /v1/chat/completions endpoint that Ollama exposes.
All functions return None on any error — callers must handle None gracefully
so the game remains playable when the LLM is unavailable.
"""
from __future__ import annotations
import os
import requests

OLLAMA_URL = os.environ.get(
    "ZORK_LLM_URL",
    "http://localhost:11434/v1/chat/completions",
)
TIMEOUT = 15  # seconds — generous for local inference


def call_ollama(
    system: str,
    user: str,
    model: str = "qwen2.5:1.5b",
    temperature: float = 0.7,
) -> str | None:
    """Call Ollama and return the assistant's reply text, or None on any error."""
    try:
        resp = requests.post(
            OLLAMA_URL,
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
