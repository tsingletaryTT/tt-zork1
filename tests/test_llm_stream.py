# tests/test_llm_stream.py
import json
import pytest
from unittest.mock import patch, MagicMock


def test_call_llm_stream_yields_chunks():
    from remix.llm import call_llm_stream

    sse_lines = [
        b'data: {"choices":[{"delta":{"content":"Hello"}}]}',
        b'data: {"choices":[{"delta":{"content":" world"}}]}',
        b'data: [DONE]',
    ]
    mock_resp = MagicMock()
    mock_resp.status_code = 200
    mock_resp.iter_lines.return_value = iter(sse_lines)

    with patch("remix.llm.requests.post", return_value=mock_resp):
        chunks = list(call_llm_stream("sys", "user"))

    assert chunks == ["Hello", " world"]


def test_call_llm_stream_empty_on_connection_error():
    from remix.llm import call_llm_stream

    with patch("remix.llm.requests.post", side_effect=Exception("network error")):
        chunks = list(call_llm_stream("sys", "user"))

    assert chunks == []


def test_call_llm_stream_empty_on_non_200():
    from remix.llm import call_llm_stream

    mock_resp = MagicMock()
    mock_resp.status_code = 503
    with patch("remix.llm.requests.post", return_value=mock_resp):
        chunks = list(call_llm_stream("sys", "user"))

    assert chunks == []


def test_call_llm_stream_skips_empty_delta():
    from remix.llm import call_llm_stream

    sse_lines = [
        b'data: {"choices":[{"delta":{}}]}',
        b'data: {"choices":[{"delta":{"content":"token"}}]}',
        b'data: [DONE]',
    ]
    mock_resp = MagicMock()
    mock_resp.status_code = 200
    mock_resp.iter_lines.return_value = iter(sse_lines)

    with patch("remix.llm.requests.post", return_value=mock_resp):
        chunks = list(call_llm_stream("sys", "user"))

    assert chunks == ["token"]


def test_remix_output_stream_calls_on_token_and_returns_text():
    from remix.output_remixer import remix_output_stream

    received = []
    sse_lines = [
        b'data: {"choices":[{"delta":{"content":"Creaks"}}]}',
        b'data: {"choices":[{"delta":{"content":" open"}}]}',
        b'data: [DONE]',
    ]
    mock_resp = MagicMock()
    mock_resp.status_code = 200
    mock_resp.iter_lines.return_value = iter(sse_lines)

    with patch("remix.llm.requests.post", return_value=mock_resp):
        result = remix_output_stream("open mailbox", "The mailbox opens.", on_token=received.append)

    assert "Creaks" in result
    assert " open" in result
    assert received == ["Creaks", " open"]


def test_remix_output_stream_fallback_when_stream_empty():
    from remix.output_remixer import remix_output_stream

    empty_stream = MagicMock()
    empty_stream.status_code = 200
    empty_stream.iter_lines.return_value = iter([b'data: [DONE]'])

    fallback_resp = MagicMock()
    fallback_resp.status_code = 200
    fallback_resp.json.return_value = {"choices": [{"message": {"content": "Fallback text"}}]}

    def mock_post(*args, **kwargs):
        return empty_stream if kwargs.get("stream") else fallback_resp

    tokens_called = []
    with patch("remix.llm.requests.post", side_effect=mock_post):
        result = remix_output_stream("cmd", "Game response.", on_token=tokens_called.append)

    assert result == "Fallback text"
    assert tokens_called == []  # on_token not called on fallback path


def test_remix_output_stream_falls_back_to_original_when_llm_down():
    from remix.output_remixer import remix_output_stream

    with patch("remix.llm.requests.post", side_effect=Exception("down")):
        result = remix_output_stream("cmd", "Original game text.", on_token=lambda t: None)

    assert result == "Original game text."
