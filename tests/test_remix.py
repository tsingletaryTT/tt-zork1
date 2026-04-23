# tests/test_remix.py
import pytest
from unittest.mock import patch, MagicMock


def test_call_ollama_returns_string_on_success():
    from remix.llm import call_ollama
    mock_resp = MagicMock()
    mock_resp.status_code = 200
    mock_resp.json.return_value = {
        "choices": [{"message": {"content": "open mailbox"}}]
    }
    with patch("remix.llm.requests.post", return_value=mock_resp):
        result = call_ollama("system", "user input", model="test-model")
    assert result == "open mailbox"


def test_call_ollama_returns_none_on_connection_error():
    from remix.llm import call_ollama
    with patch("remix.llm.requests.post", side_effect=Exception("connection refused")):
        result = call_ollama("system", "user input", model="test-model")
    assert result is None


def test_call_ollama_returns_none_on_non_200():
    from remix.llm import call_ollama
    mock_resp = MagicMock()
    mock_resp.status_code = 500
    with patch("remix.llm.requests.post", return_value=mock_resp):
        result = call_ollama("system", "user", model="test-model")
    assert result is None


def test_router_map_uses_cheap_model():
    from remix.router import route
    model = route("map")
    assert "1.5b" in model or "0.5b" in model


def test_router_remix_uses_rich_model():
    from remix.router import route
    assert route("remix") == "qwen2.5:7b"


def test_router_override_via_env(monkeypatch):
    monkeypatch.setenv("ZORK_LLM_MODEL", "custom-model:3b")
    # Need to re-import to pick up env var (module-level _OVERRIDE)
    import importlib
    import remix.router as router_mod
    importlib.reload(router_mod)
    assert router_mod.route("map") == "custom-model:3b"
    assert router_mod.route("remix") == "custom-model:3b"
    # Restore
    monkeypatch.delenv("ZORK_LLM_MODEL")
    importlib.reload(router_mod)


def test_input_mapper_passthrough_on_llm_failure():
    from remix.input_mapper import map_input
    with patch("remix.input_mapper.call_ollama", return_value=None):
        result = map_input("open mailbox")
    assert result == "open mailbox"


def test_input_mapper_uses_llm_result():
    from remix.input_mapper import map_input
    with patch("remix.input_mapper.call_ollama", return_value="open mailbox"):
        result = map_input("open the mailbox with my teeth")
    assert result == "open mailbox"


def test_input_mapper_strips_whitespace():
    from remix.input_mapper import map_input
    with patch("remix.input_mapper.call_ollama", return_value="  north  "):
        result = map_input("go north")
    assert result == "north"
