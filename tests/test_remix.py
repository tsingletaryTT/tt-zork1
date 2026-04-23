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


def test_output_remixer_passthrough_on_llm_failure():
    from remix.output_remixer import remix_output
    with patch("remix.output_remixer.call_ollama", return_value=None):
        result = remix_output("open mailbox with my teeth", "Opening reveals a leaflet.")
    assert result == "Opening reveals a leaflet."


def test_output_remixer_returns_llm_result():
    from remix.output_remixer import remix_output
    creative = "The mailbox creaks open. Inside: a leaflet."
    with patch("remix.output_remixer.call_ollama", return_value=creative):
        result = remix_output("open mailbox with my teeth", "Opening reveals a leaflet.")
    assert result == creative


def test_output_remixer_passthrough_on_empty_llm():
    from remix.output_remixer import remix_output
    with patch("remix.output_remixer.call_ollama", return_value=""):
        result = remix_output("look", "West of House\nYou are standing...")
    assert "West of House" in result


def test_ascii_artist_returns_string():
    from remix.ascii_artist import AsciiArtist
    artist = AsciiArtist()
    mock_art = "   _____\n  |     |\n  |_____|"
    with patch("remix.ascii_artist.call_ollama", return_value=mock_art):
        result = artist.get("West of House", "You are standing in a field.")
    assert isinstance(result, str)
    assert len(result) > 0


def test_ascii_artist_caches_per_room():
    from remix.ascii_artist import AsciiArtist
    artist = AsciiArtist()
    mock_art = "   ___\n  |   |\n  |___|"
    with patch("remix.ascii_artist.call_ollama", return_value=mock_art) as mock_llm:
        artist.get("Forest", "Trees everywhere.")
        artist.get("Forest", "Trees everywhere.")
    assert mock_llm.call_count == 1   # second call is cached


def test_ascii_artist_returns_empty_on_llm_failure():
    from remix.ascii_artist import AsciiArtist
    artist = AsciiArtist()
    with patch("remix.ascii_artist.call_ollama", return_value=None) as mock_llm:
        result = artist.get("Cave", "Dark cave.")
        _ = artist.get("Cave", "Dark cave.")   # second call — must not re-invoke LLM
    assert result == ""
    assert mock_llm.call_count == 1   # empty string was cached, no retry
