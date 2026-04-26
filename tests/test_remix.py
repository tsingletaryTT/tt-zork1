# tests/test_remix.py
import pytest
from unittest.mock import patch, MagicMock


def test_call_llm_returns_string_on_success():
    from remix.llm import call_llm
    mock_resp = MagicMock()
    mock_resp.status_code = 200
    mock_resp.json.return_value = {
        "choices": [{"message": {"content": "open mailbox"}}]
    }
    with patch("remix.llm.requests.post", return_value=mock_resp):
        result = call_llm("system", "user input", model="test-model")
    assert result == "open mailbox"


def test_call_llm_returns_none_on_connection_error():
    from remix.llm import call_llm
    with patch("remix.llm.requests.post", side_effect=Exception("connection refused")):
        result = call_llm("system", "user input", model="test-model")
    assert result is None


def test_call_llm_returns_none_on_non_200():
    from remix.llm import call_llm
    mock_resp = MagicMock()
    mock_resp.status_code = 500
    with patch("remix.llm.requests.post", return_value=mock_resp):
        result = call_llm("system", "user", model="test-model")
    assert result is None


def test_router_map_uses_cheap_model():
    import importlib, remix.router as router_mod
    importlib.reload(router_mod)
    # No server running → autodetect returns "" → falls back to task table.
    with patch("remix.router._detect_running_model", return_value=""):
        model = router_mod.route("map")
    assert "1B" in model or "1b" in model


def test_router_remix_uses_rich_model():
    import importlib, remix.router as router_mod
    importlib.reload(router_mod)
    with patch("remix.router._detect_running_model", return_value=""):
        assert router_mod.route("remix") == "meta-llama/Llama-3.1-8B-Instruct"


def test_router_autodetect_single_model(monkeypatch):
    """When exactly one model is running, all tasks use it."""
    import importlib, remix.router as router_mod
    importlib.reload(router_mod)
    with patch("remix.router._detect_running_model", return_value="my-org/Llama-3.3-70B-Instruct"):
        assert router_mod.route("map") == "my-org/Llama-3.3-70B-Instruct"
        assert router_mod.route("remix") == "my-org/Llama-3.3-70B-Instruct"
        assert router_mod.route("art") == "my-org/Llama-3.3-70B-Instruct"


def test_router_autodetect_no_server():
    """When the server is down, autodetect silently returns '' and routing falls back."""
    import importlib, remix.router as router_mod
    importlib.reload(router_mod)
    mock_get = MagicMock(side_effect=Exception("connection refused"))
    with patch("remix.router._detect_running_model", return_value=""):
        assert "Instruct" in router_mod.route("remix")


def test_router_override_via_env(monkeypatch):
    monkeypatch.setenv("ZORK_LLM_MODEL", "custom-model:3b")
    import importlib
    import remix.router as router_mod
    importlib.reload(router_mod)
    assert router_mod.route("map") == "custom-model:3b"
    assert router_mod.route("remix") == "custom-model:3b"
    # Restore
    monkeypatch.delenv("ZORK_LLM_MODEL")
    importlib.reload(router_mod)


def test_detected_model_returns_single_model():
    """detected_model() returns the model name when exactly one is served."""
    import importlib, remix.router as router_mod
    importlib.reload(router_mod)
    mock_resp = MagicMock()
    mock_resp.status_code = 200
    mock_resp.json.return_value = {"data": [{"id": "my-org/Llama-3.3-70B-Instruct"}]}
    with patch("requests.get", return_value=mock_resp):
        result = router_mod.detected_model()
    assert result == "my-org/Llama-3.3-70B-Instruct"


def test_detected_model_empty_on_multiple_models():
    """When multiple models are served, detected_model() returns '' to preserve routing."""
    import importlib, remix.router as router_mod
    importlib.reload(router_mod)
    mock_resp = MagicMock()
    mock_resp.status_code = 200
    mock_resp.json.return_value = {"data": [{"id": "model-a"}, {"id": "model-b"}]}
    with patch("requests.get", return_value=mock_resp):
        result = router_mod.detected_model()
    assert result == ""


def test_input_mapper_passthrough_on_llm_failure():
    from remix.input_mapper import map_input
    with patch("remix.input_mapper.call_llm", return_value=None):
        result = map_input("open mailbox")
    assert result == "open mailbox"


def test_input_mapper_uses_llm_result():
    from remix.input_mapper import map_input
    with patch("remix.input_mapper.call_llm", return_value="open mailbox"):
        result = map_input("open the mailbox with my teeth")
    assert result == "open mailbox"


def test_input_mapper_strips_whitespace():
    # Freeform prose goes through the LLM; verify its result is stripped.
    from remix.input_mapper import map_input
    with patch("remix.input_mapper.call_llm", return_value="  north  "):
        result = map_input("head that way up")
    assert result == "north"


def test_input_mapper_literal_bypass():
    # Exact Zork commands never reach the LLM at all.
    from remix.input_mapper import map_input, _is_literal_command
    assert _is_literal_command("go north")
    assert _is_literal_command("north")
    assert _is_literal_command("take lamp")
    assert _is_literal_command("open mailbox")
    assert _is_literal_command("attack troll with sword")
    assert _is_literal_command("inventory")
    assert _is_literal_command("look")
    # Prose inputs must NOT be bypassed
    assert not _is_literal_command("open the mailbox with my teeth")
    assert not _is_literal_command("I want to go north")
    assert not _is_literal_command("grab that leaflet")


def test_output_remixer_passthrough_on_llm_failure():
    from remix.output_remixer import remix_output
    with patch("remix.output_remixer.call_llm", return_value=None):
        result = remix_output("open mailbox with my teeth", "Opening reveals a leaflet.")
    assert result == "Opening reveals a leaflet."


def test_output_remixer_returns_llm_result():
    from remix.output_remixer import remix_output
    creative = "The mailbox creaks open. Inside: a leaflet."
    with patch("remix.output_remixer.call_llm", return_value=creative):
        result = remix_output("open mailbox with my teeth", "Opening reveals a leaflet.")
    assert result == creative


def test_output_remixer_passthrough_on_empty_llm():
    from remix.output_remixer import remix_output
    with patch("remix.output_remixer.call_llm", return_value=""):
        result = remix_output("look", "West of House\nYou are standing...")
    assert "West of House" in result


def test_ascii_artist_returns_string():
    from remix.ascii_artist import AsciiArtist
    artist = AsciiArtist()
    mock_art = "   _____\n  |     |\n  |_____|"
    with patch("remix.ascii_artist.call_llm", return_value=mock_art):
        result = artist.get("West of House", "You are standing in a field.")
    assert isinstance(result, str)
    assert len(result) > 0


def test_ascii_artist_caches_per_room():
    from remix.ascii_artist import AsciiArtist
    artist = AsciiArtist()
    mock_art = "   ___\n  |   |\n  |___|"
    with patch("remix.ascii_artist.call_llm", return_value=mock_art) as mock_llm:
        artist.get("Forest", "Trees everywhere.")
        artist.get("Forest", "Trees everywhere.")
    assert mock_llm.call_count == 1   # second call is cached


def test_ascii_artist_returns_empty_on_llm_failure():
    from remix.ascii_artist import AsciiArtist
    artist = AsciiArtist()
    with patch("remix.ascii_artist.call_llm", return_value=None) as mock_llm:
        result = artist.get("Cave", "Dark cave.")
        _ = artist.get("Cave", "Dark cave.")   # second call — must not re-invoke LLM
    assert result == ""
    assert mock_llm.call_count == 1   # empty string was cached, no retry


def test_narrative_enhancer_generates_postcard():
    from remix.narrative_enhancer import NarrativeEnhancer
    ne = NarrativeEnhancer()
    mock_card = "I arrived at the white house as evening fell..."
    with patch("remix.narrative_enhancer.call_llm", return_value=mock_card):
        ne.record("West of House", "arrival", "First location")
    cards = ne.get_postcards()
    assert len(cards) == 1
    assert cards[0]["text"] == mock_card


def test_narrative_enhancer_no_duplicate_moment_types():
    from remix.narrative_enhancer import NarrativeEnhancer
    ne = NarrativeEnhancer()
    with patch("remix.narrative_enhancer.call_llm", return_value="postcard text"):
        ne.record("West of House", "arrival", "first")
        ne.record("North of House", "arrival", "second")  # same type, ignored
    assert len(ne.get_postcards()) == 1


def test_personas_has_four_entries():
    from remix.personas import PERSONAS
    assert len(PERSONAS) == 4


def test_personas_have_required_keys():
    from remix.personas import PERSONAS
    for p in PERSONAS.values():
        assert "name" in p
        assert "system_prompt" in p


def test_narrative_enhancer_record_survives_llm_exception():
    from remix.narrative_enhancer import NarrativeEnhancer
    ne = NarrativeEnhancer()
    with patch("remix.narrative_enhancer.call_llm", side_effect=RuntimeError("timeout")):
        ne.record("West of House", "arrival", "first time")  # must not raise
    assert ne.get_postcards() == []


def test_remix_layer_process_calls_mapper_and_remixer():
    from remix.mode import RemixLayer
    layer = RemixLayer()
    with patch("remix.mode.map_input", return_value="open mailbox") as mock_map, \
         patch("remix.mode.remix_output", return_value="Creative response\n") as mock_remix, \
         patch.object(layer._artist, "get", return_value=""):
        result = layer.process("open the mailbox please", "The mailbox is now open.")
    mock_map.assert_called_once_with("open the mailbox please")
    mock_remix.assert_called_once()
    assert result == "Creative response\n"


def test_remix_layer_passthrough_when_inactive():
    from remix.mode import RemixLayer
    layer = RemixLayer()
    layer.active = False
    result = layer.process("open mailbox", "The mailbox is now open.")
    assert result == "The mailbox is now open."


def test_remix_layer_on_game_end_no_postcards(capsys):
    """on_game_end() prints nothing when no postcards have been collected."""
    from remix.mode import RemixLayer
    layer = RemixLayer()
    layer.on_game_end()
    assert capsys.readouterr().out == ""


def test_remix_layer_render_postcards_empty():
    """render_postcards() returns empty string when no postcards collected."""
    from remix.mode import RemixLayer
    layer = RemixLayer()
    assert layer.render_postcards() == ""
