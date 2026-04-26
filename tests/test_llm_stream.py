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
