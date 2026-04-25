# TUI Status Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `--tui` flag to `play.py` that launches a Textual-based full-terminal UI showing live LLM token streaming (colorized), room ASCII art, and hardware telemetry — while leaving the plain terminal mode fully intact.

**Architecture:** `ZMachineTuiApp(App)` wraps the existing engine and remix layer unchanged. The game loop runs in a background thread posting to Textual's event system. `call_llm_stream()` feeds tokens to the THINKING panel. A `set_interval` worker polls `tt-smi -s` for hardware state. Right panel is a state machine: THINKING → ART → HARDWARE.

**Tech Stack:** Textual 8.2.4 (installed), `requests` stream=True for SSE, `ttlang.zmachine_v3.ZMachineV3` for vocabulary extraction, `subprocess` for `tt-smi -s`.

---

## File Map

**New files:**
```
tui/__init__.py
tui/app.py            — ZMachineTuiApp(App): messages, layout, game thread
tui/game_pane.py      — left RichLog + Input widget
tui/context_pane.py   — right panel (THINKING/ART/HARDWARE) + classify_token()
tui/hw_poller.py      — HardwareSnapshot dataclass + poll()
tui/vocabulary.py     — extract_vocabulary(zm) → frozenset[str]
tests/test_llm_stream.py
tests/test_tui_vocabulary.py
tests/test_hw_poller.py
tests/test_tui_colorizer.py
```

**Modified files:**
```
remix/llm.py          — add call_llm_stream()
remix/output_remixer.py — add remix_output_stream()
play.py               — add --tui flag
```

---

## Task 1: `call_llm_stream()` in `remix/llm.py`

**Files:**
- Modify: `remix/llm.py`
- Create: `tests/test_llm_stream.py`

- [ ] **Step 1: Write the failing test**

Create `tests/test_llm_stream.py`:

```python
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

    # delta with no "content" key is silently skipped
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
```

- [ ] **Step 2: Run test to confirm it fails**

```bash
cd /home/ttuser/code/tt-zork1
source ~/code/tt-lang/build/env/activate
pytest tests/test_llm_stream.py -v 2>&1 | head -20
```

Expected: `ImportError` — `cannot import name 'call_llm_stream' from 'remix.llm'`

- [ ] **Step 3: Implement `call_llm_stream()`**

Add to `remix/llm.py` after the existing `call_llm()` function:

```python
import json
from typing import Iterator


def call_llm_stream(
    system: str,
    user: str,
    model: str = "meta-llama/Llama-3.1-8B-Instruct",
    temperature: float = 0.7,
) -> Iterator[str]:
    """Yield text chunks as they stream from the server (SSE).

    Parses OpenAI-compatible SSE: data: {"choices":[{"delta":{"content":"tok"}}]}
    Stops on `data: [DONE]`. Yields empty iterator on any error.
    """
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
                "stream": True,
            },
            stream=True,
            timeout=TIMEOUT,
        )
        if resp.status_code != 200:
            return
        for raw in resp.iter_lines():
            if not raw:
                continue
            line = raw.decode("utf-8") if isinstance(raw, bytes) else raw
            if not line.startswith("data: "):
                continue
            payload = line[6:]
            if payload.strip() == "[DONE]":
                return
            try:
                chunk = json.loads(payload)
                content = chunk["choices"][0]["delta"].get("content", "")
                if content:
                    yield content
            except (KeyError, IndexError, json.JSONDecodeError):
                continue
    except Exception:
        return
```

Also add `import json` and `from typing import Iterator` at the top of `remix/llm.py`.

- [ ] **Step 4: Run tests to confirm they pass**

```bash
pytest tests/test_llm_stream.py -v
```

Expected: `4 passed`

- [ ] **Step 5: Commit**

```bash
git add remix/llm.py tests/test_llm_stream.py
git commit -m "feat: add call_llm_stream() SSE generator to remix/llm.py"
```

---

## Task 2: `remix_output_stream()` in `remix/output_remixer.py`

**Files:**
- Modify: `remix/output_remixer.py`
- Modify: `tests/test_llm_stream.py` (add tests)

- [ ] **Step 1: Write the failing tests**

Append to `tests/test_llm_stream.py`:

```python
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
```

- [ ] **Step 2: Run tests to confirm they fail**

```bash
pytest tests/test_llm_stream.py::test_remix_output_stream_calls_on_token_and_returns_text -v
```

Expected: `ImportError` — `cannot import name 'remix_output_stream'`

- [ ] **Step 3: Implement `remix_output_stream()`**

Add to `remix/output_remixer.py` after the existing `remix_output()` function:

```python
from typing import Callable
from remix.llm import call_llm_stream


def remix_output_stream(
    user_input: str,
    game_response: str,
    on_token: Callable[[str], None],
) -> str:
    """Stream remix tokens to on_token callback; return full remixed text.

    Falls back to blocking call_llm() if the stream yields nothing. Always
    returns the complete final string so the caller has the result regardless
    of which path was taken.
    """
    user_msg = (
        f"PLAYER: {user_input}\n"
        f"GAME:\n{game_response}"
    )
    tokens: list[str] = []
    for chunk in call_llm_stream(
        system=_SYSTEM_PROMPT,
        user=user_msg,
        model=route("remix"),
        temperature=0.8,
    ):
        on_token(chunk)
        tokens.append(chunk)

    if tokens:
        result = "".join(tokens).strip()
        return result if result else game_response

    # Stream yielded nothing — fall back to blocking call
    result = call_llm(
        system=_SYSTEM_PROMPT,
        user=user_msg,
        model=route("remix"),
        temperature=0.8,
    )
    return result.strip() if result and result.strip() else game_response
```

Also add `from typing import Callable` and `from remix.llm import call_llm_stream` at the top of `remix/output_remixer.py`. The existing `from remix.llm import call_llm` import should remain.

- [ ] **Step 4: Run all stream tests**

```bash
pytest tests/test_llm_stream.py -v
```

Expected: `7 passed`

- [ ] **Step 5: Commit**

```bash
git add remix/output_remixer.py tests/test_llm_stream.py
git commit -m "feat: add remix_output_stream() streaming variant to output_remixer"
```

---

## Task 3: `tui/vocabulary.py` — Z-machine dictionary extraction

**Files:**
- Create: `tui/__init__.py`
- Create: `tui/vocabulary.py`
- Create: `tests/test_tui_vocabulary.py`

- [ ] **Step 1: Write the failing tests**

Create `tests/test_tui_vocabulary.py`:

```python
# tests/test_tui_vocabulary.py
"""Vocabulary extraction from the Z-machine dictionary table."""
from pathlib import Path
import pytest


GAME_PATH = Path("game/zork1.z3")


def test_extract_vocabulary_returns_nonempty_frozenset():
    from ttlang.zmachine_v3 import ZMachineV3
    from tui.vocabulary import extract_vocabulary

    zm = ZMachineV3(GAME_PATH.read_bytes())
    vocab = extract_vocabulary(zm)

    assert isinstance(vocab, frozenset)
    assert len(vocab) > 100


def test_extract_vocabulary_contains_known_words():
    from ttlang.zmachine_v3 import ZMachineV3
    from tui.vocabulary import extract_vocabulary

    zm = ZMachineV3(GAME_PATH.read_bytes())
    vocab = extract_vocabulary(zm)

    assert "mailbox" in vocab
    assert "north" in vocab


def test_extract_vocabulary_returns_empty_frozenset_on_error():
    from tui.vocabulary import extract_vocabulary

    class BrokenZM:
        dictionary_addr = 0

        def read_byte(self, addr: int) -> int:
            raise RuntimeError("boom")

        def read_word(self, addr: int) -> int:
            raise RuntimeError("boom")

        def decode_zstring(self, addr: int) -> str:
            return ""

    assert extract_vocabulary(BrokenZM()) == frozenset()
```

- [ ] **Step 2: Run tests to confirm they fail**

```bash
pytest tests/test_tui_vocabulary.py -v
```

Expected: `ModuleNotFoundError: No module named 'tui'`

- [ ] **Step 3: Create `tui/__init__.py` and `tui/vocabulary.py`**

Create `tui/__init__.py` (empty):

```python
```

Create `tui/vocabulary.py`:

```python
# tui/vocabulary.py
"""Extract the Z-machine dictionary from a loaded game binary.

The dictionary table (Z-machine V3 spec §13) is at the byte address stored
in header word 0x08. Its layout:
    1 byte:  number of word separators (n)
    n bytes: the separator characters themselves
    1 byte:  entry length (typically 7 for V3)
    2 bytes: entry count (big-endian)
    entry_count × entry_length bytes: the entries

Each entry starts with a 4-byte Z-string (two Z-machine words). We decode
every entry and return the resulting words as a frozenset of lowercase strings.

If anything goes wrong (malformed game, unsupported format), returns frozenset()
so callers degrade gracefully without crashing.
"""
from __future__ import annotations


def extract_vocabulary(zm) -> frozenset[str]:
    """Read the Z-machine dictionary from zm and return a frozenset of words.

    zm must expose: .dictionary_addr (int), .read_byte(addr), .read_word(addr),
    .decode_zstring(addr) — all satisfied by ZMachineV3 from ttlang/zmachine_v3.py.
    """
    try:
        addr = zm.dictionary_addr
        sep_count = zm.read_byte(addr)
        addr += 1 + sep_count          # skip separator-count byte + separators
        entry_length = zm.read_byte(addr)
        addr += 1
        entry_count = zm.read_word(addr)
        addr += 2

        words: set[str] = set()
        for i in range(entry_count):
            entry_addr = addr + i * entry_length
            word = zm.decode_zstring(entry_addr).strip().lower()
            if word:
                words.add(word)
        return frozenset(words)
    except Exception:
        return frozenset()
```

- [ ] **Step 4: Run tests to confirm they pass**

```bash
pytest tests/test_tui_vocabulary.py -v
```

Expected: `3 passed`

- [ ] **Step 5: Commit**

```bash
git add tui/__init__.py tui/vocabulary.py tests/test_tui_vocabulary.py
git commit -m "feat: add tui/vocabulary.py — Z-machine dictionary extraction"
```

---

## Task 4: `tui/hw_poller.py` — hardware telemetry

**Files:**
- Create: `tui/hw_poller.py`
- Create: `tests/test_hw_poller.py`

The `tt-smi -s` JSON structure (observed on this system):
```json
{
  "device_info": [{
    "smbus_telem": {
      "ASIC_TEMPERATURE": "0x26ef22",
      "TDP": "0x11",
      "AICLK": "0x320"
    }
  }]
}
```

Temperature: top byte of ASIC_TEMPERATURE raw value (0x26ef22 >> 16 = 0x26 = 38°C).
Power: TDP raw integer in watts (0x11 = 17W).
Tensix/RISC-V utilisation and DRAM bandwidth are not in the basic snapshot; return -1.

- [ ] **Step 1: Write the failing tests**

Create `tests/test_hw_poller.py`:

```python
# tests/test_hw_poller.py
import json
import pytest
from unittest.mock import patch, MagicMock


_SAMPLE_JSON = {
    "device_info": [{
        "smbus_telem": {
            "ASIC_TEMPERATURE": "0x26ef22",
            "TDP": "0x11",
            "AICLK": "0x320",
        }
    }]
}


def test_poll_extracts_temperature():
    from tui.hw_poller import poll

    with patch("tui.hw_poller.subprocess.run") as mock_run:
        mock_run.return_value = MagicMock(returncode=0, stdout=json.dumps(_SAMPLE_JSON))
        snap = poll()

    # 0x26ef22 >> 16 = 0x26 = 38
    assert snap.temp_c == pytest.approx(38.0)


def test_poll_extracts_power():
    from tui.hw_poller import poll

    with patch("tui.hw_poller.subprocess.run") as mock_run:
        mock_run.return_value = MagicMock(returncode=0, stdout=json.dumps(_SAMPLE_JSON))
        snap = poll()

    # 0x11 = 17
    assert snap.power_w == pytest.approx(17.0)


def test_poll_returns_zeroed_when_tt_smi_missing():
    from tui.hw_poller import poll

    with patch("tui.hw_poller.subprocess.run", side_effect=FileNotFoundError):
        snap = poll()

    assert snap.temp_c == -1
    assert snap.tensix_pct == -1
    assert snap.power_w == -1


def test_poll_returns_zeroed_on_nonzero_exit():
    from tui.hw_poller import poll

    with patch("tui.hw_poller.subprocess.run") as mock_run:
        mock_run.return_value = MagicMock(returncode=1, stdout="")
        snap = poll()

    assert snap.temp_c == -1


def test_poll_returns_zeroed_on_malformed_json():
    from tui.hw_poller import poll

    with patch("tui.hw_poller.subprocess.run") as mock_run:
        mock_run.return_value = MagicMock(returncode=0, stdout="not json {")
        snap = poll()

    assert snap.temp_c == -1


def test_poll_never_raises():
    from tui.hw_poller import poll

    with patch("tui.hw_poller.subprocess.run", side_effect=PermissionError("denied")):
        snap = poll()  # must not raise

    assert snap.tensix_pct == -1
```

- [ ] **Step 2: Run tests to confirm they fail**

```bash
pytest tests/test_hw_poller.py -v
```

Expected: `ModuleNotFoundError: No module named 'tui.hw_poller'`

- [ ] **Step 3: Implement `tui/hw_poller.py`**

Create `tui/hw_poller.py`:

```python
# tui/hw_poller.py
"""Poll Tenstorrent hardware telemetry via `tt-smi -s`.

HardwareSnapshot holds one reading. poll() never raises — callers get a
fully-zeroed snapshot (all -1) when hardware is unavailable.

The stage_label field is NOT set by poll(); it is filled in by ZMachineTuiApp
before posting a HardwareUpdate message, since the app knows which engine is active.
"""
from __future__ import annotations

import json
import subprocess
from dataclasses import dataclass, replace


@dataclass
class HardwareSnapshot:
    tensix_pct: float       # 0–100; -1 if unavailable
    riscv_pct: float        # 0–100; -1 if unavailable
    dram_read_gbps: float   # -1 if unavailable
    dram_write_gbps: float  # -1 if unavailable
    temp_c: float           # degrees C; -1 if unavailable
    power_w: float          # watts; -1 if unavailable
    stage_label: str        # filled by ZMachineTuiApp, not by poll()


_ZEROED = HardwareSnapshot(-1.0, -1.0, -1.0, -1.0, -1.0, -1.0, "")


def poll() -> HardwareSnapshot:
    """Run `tt-smi -s`, parse JSON, return a HardwareSnapshot.

    Returns a zeroed snapshot with all numeric fields -1 if tt-smi is
    unavailable, returns a non-zero exit code, or produces malformed JSON.
    Never raises.
    """
    try:
        result = subprocess.run(
            ["tt-smi", "-s"],
            capture_output=True,
            text=True,
            timeout=3,
        )
        if result.returncode != 0:
            return replace(_ZEROED)
        data = json.loads(result.stdout)
        return _parse(data)
    except Exception:
        return replace(_ZEROED)


def _parse(data: dict) -> HardwareSnapshot:
    """Extract fields from the tt-smi -s JSON structure."""
    try:
        devices = data.get("device_info", [])
        if not devices:
            return replace(_ZEROED)
        telem = devices[0].get("smbus_telem", {})

        temp_c = _hex_to_temp(telem.get("ASIC_TEMPERATURE"))
        power_w = _hex_int(telem.get("TDP"))

        return HardwareSnapshot(
            tensix_pct=-1.0,
            riscv_pct=-1.0,
            dram_read_gbps=-1.0,
            dram_write_gbps=-1.0,
            temp_c=temp_c,
            power_w=power_w,
            stage_label="",
        )
    except Exception:
        return replace(_ZEROED)


def _hex_int(value) -> float:
    """Parse a hex string like '0x11' to float, return -1 on failure."""
    try:
        return float(int(str(value), 16))
    except (TypeError, ValueError):
        return -1.0


def _hex_to_temp(value) -> float:
    """Extract temperature °C from ASIC_TEMPERATURE raw hex value.

    The Tenstorrent ASIC_TEMPERATURE telemetry field is a 24-bit value whose
    top byte is the temperature in degrees Celsius (e.g. 0x26ef22 → 0x26 = 38°C).
    """
    try:
        raw = int(str(value), 16)
        return float((raw >> 16) & 0xFF)
    except (TypeError, ValueError):
        return -1.0
```

- [ ] **Step 4: Run tests to confirm they pass**

```bash
pytest tests/test_hw_poller.py -v
```

Expected: `6 passed`

- [ ] **Step 5: Commit**

```bash
git add tui/hw_poller.py tests/test_hw_poller.py
git commit -m "feat: add tui/hw_poller.py — HardwareSnapshot + tt-smi poll()"
```

---

## Task 5: `tui/game_pane.py` — left scrolling panel

**Files:**
- Create: `tui/game_pane.py`

No dedicated unit tests — this is a thin Textual widget. Integration is tested in Task 9.

- [ ] **Step 1: Create `tui/game_pane.py`**

```python
# tui/game_pane.py
"""GamePane — left panel of the TUI: scrolling game text and player input.

Receives GameText messages from the game thread via the app's event loop.
The Input widget at the bottom captures player commands; Input.Submitted
events bubble up to ZMachineTuiApp for queuing.
"""
from __future__ import annotations

from textual.app import ComposeResult
from textual.widget import Widget
from textual.widgets import Input, RichLog


class GamePane(Widget):
    """Scrolling game output + player input field."""

    DEFAULT_CSS = """
    GamePane {
        layout: vertical;
        border-right: solid $primary-darken-2;
    }
    #game-log {
        width: 100%;
        height: 1fr;
    }
    #game-input {
        width: 100%;
        height: 3;
        border-top: solid $primary-darken-2;
    }
    """

    def compose(self) -> ComposeResult:
        yield RichLog(id="game-log", highlight=False, markup=True, wrap=True)
        yield Input(placeholder="> type a command", id="game-input")

    def write_game_text(self, text: str) -> None:
        """Append Z-machine or remixed output (default colour, no markup)."""
        log = self.query_one("#game-log", RichLog)
        for line in text.rstrip().splitlines():
            log.write(f"[#e8f0f2]{line}[/]")
        log.write("")  # blank line between turns

    def write_player_text(self, text: str) -> None:
        """Append player input echo in dim grey."""
        log = self.query_one("#game-log", RichLog)
        for line in text.rstrip().splitlines():
            log.write(f"[#607d8b]{line}[/]")
```

- [ ] **Step 2: Commit**

```bash
git add tui/game_pane.py
git commit -m "feat: add tui/game_pane.py — RichLog + Input widget"
```

---

## Task 6: `tui/context_pane.py` + colorizer tests

**Files:**
- Create: `tui/context_pane.py`
- Create: `tests/test_tui_colorizer.py`

### Token colorizer rules (applied in this priority order):
1. **Teal `#4fd1c5`**: word (stripped of punctuation, lowercased) is in the vocabulary frozenset
2. **Pink `#ec96b8`**: word length ≥ 8 AND ends in `ous/ful/ing/ish/ent/ive/ly`
3. **Amber `#f4c471`**: capitalized mid-sentence (not sentence-start) OR previous token is `a/an/the/this/that`
4. **Dim grey `#607d8b`**: word is in the ~20-word stopword list
5. **Default `#e8f0f2`**: everything else

- [ ] **Step 1: Write the failing colorizer tests**

Create `tests/test_tui_colorizer.py`:

```python
# tests/test_tui_colorizer.py
"""Tests for context_pane token colorizer."""
import pytest


def test_known_word_is_teal():
    from tui.context_pane import classify_token
    vocab = frozenset({"mailbox", "leaflet", "lamp"})
    assert classify_token("mailbox", vocab) == "#4fd1c5"


def test_known_word_with_punctuation_is_teal():
    from tui.context_pane import classify_token
    vocab = frozenset({"mailbox"})
    # comma attached — strip before lookup
    assert classify_token("mailbox,", vocab) == "#4fd1c5"


def test_stopword_is_grey():
    from tui.context_pane import classify_token
    for w in ("the", "a", "an", "in", "of", "and", "or"):
        assert classify_token(w, frozenset()) == "#607d8b", f"Expected grey for '{w}'"


def test_vivid_suffix_long_word_is_pink():
    from tui.context_pane import classify_token
    # "terrifying" = 10 chars, ends in "ing"
    assert classify_token("terrifying", frozenset()) == "#ec96b8"
    # "malevolent" = 10 chars, ends in "ent"
    assert classify_token("malevolent", frozenset()) == "#ec96b8"


def test_short_vivid_suffix_is_not_pink():
    from tui.context_pane import classify_token
    # "going" = 5 chars, ends in "ing" but length < 8 → not pink
    result = classify_token("going", frozenset())
    assert result != "#ec96b8"


def test_capitalized_mid_sentence_is_amber():
    from tui.context_pane import classify_token
    # "Darkness" mid-sentence (not start), not a known vocab word
    assert classify_token("Darkness", frozenset(), is_sentence_start=False) == "#f4c471"


def test_capitalized_at_sentence_start_is_not_amber():
    from tui.context_pane import classify_token
    result = classify_token("The", frozenset(), is_sentence_start=True)
    # "The" is a stopword → grey
    assert result == "#607d8b"


def test_follows_article_is_amber():
    from tui.context_pane import classify_token
    # "castle" follows "the"; not a vocab word, not vivid, not stopword
    assert classify_token("castle", frozenset(), prev_token="the") == "#f4c471"


def test_empty_vocabulary_no_teal():
    from tui.context_pane import classify_token
    # "mailbox" with empty vocab should NOT be teal
    result = classify_token("mailbox", frozenset())
    assert result != "#4fd1c5"


def test_vocab_takes_priority_over_vivid():
    from tui.context_pane import classify_token
    # "horrifying" (9 chars, "ing") is also in vocab → teal wins
    vocab = frozenset({"horrifying"})
    assert classify_token("horrifying", vocab) == "#4fd1c5"
```

- [ ] **Step 2: Run tests to confirm they fail**

```bash
pytest tests/test_tui_colorizer.py -v
```

Expected: `ModuleNotFoundError: No module named 'tui.context_pane'`

- [ ] **Step 3: Implement `classify_token()` and `tui/context_pane.py`**

Create `tui/context_pane.py`:

```python
# tui/context_pane.py
"""ContextPane — right panel with three states: THINKING, ART, HARDWARE.

State machine:
    HARDWARE (default)
      → THINKING  on StreamStart message
          → ART       on StreamDone(task="art")
          → HARDWARE  on StreamDone(task=other)
      → ART           on ShowArt message
          → THINKING  on next StreamStart

Token colorizer rules (classify_token):
    1. teal  #4fd1c5  — word in Z-machine vocabulary frozenset
    2. pink  #ec96b8  — length ≥ 8 AND ends in vivid suffix
    3. amber #f4c471  — capitalised mid-sentence OR follows a/an/the/this/that
    4. grey  #607d8b  — articles, prepositions, conjunctions
    5. default #e8f0f2 — everything else
"""
from __future__ import annotations

import asyncio
from textual.app import ComposeResult
from textual.widget import Widget
from textual.widgets import Static
from textual.scroll_view import ScrollView

# ---------------------------------------------------------------------------
# Token colorizer
# ---------------------------------------------------------------------------

_STOPWORDS = frozenset({
    "a", "an", "the", "this", "that", "these", "those",
    "is", "are", "was", "were", "be", "been",
    "in", "on", "at", "to", "of", "and", "or", "but",
    "it", "you", "i", "we", "he", "she", "they",
    "with", "for", "from", "by", "not",
})

_VIVID_ENDS = ("ous", "ful", "ing", "ish", "ent", "ive", "ly")

_CONTEXT_NOUNS = frozenset({"a", "an", "the", "this", "that"})


def classify_token(
    token: str,
    vocabulary: frozenset[str],
    prev_token: str = "",
    is_sentence_start: bool = False,
) -> str:
    """Return a hex color code for this display token.

    token         — one whitespace-delimited word (may include attached punctuation)
    vocabulary    — game dictionary frozenset from tui/vocabulary.py
    prev_token    — the word immediately before this one (lowercased, bare)
    is_sentence_start — True if this is the first token of a sentence
    """
    clean = token.strip(".,!?;:'\"-()[]").lower()
    if not clean:
        return "#e8f0f2"

    # Rule 1: game vocabulary (teal)
    if clean in vocabulary:
        return "#4fd1c5"

    # Rule 4: stopwords (dim grey) — checked before vivid/noun to keep common
    # words dim even when they technically have vivid suffixes (e.g. "going")
    if clean in _STOPWORDS:
        return "#607d8b"

    # Rule 2: vivid suffix + minimum length (pink)
    if len(clean) >= 8 and any(clean.endswith(s) for s in _VIVID_ENDS):
        return "#ec96b8"

    # Rule 3: capitalized mid-sentence or follows a context noun (amber)
    if (token[0].isupper() and not is_sentence_start) or prev_token.lower() in _CONTEXT_NOUNS:
        return "#f4c471"

    return "#e8f0f2"


# ---------------------------------------------------------------------------
# ContextPane widget
# ---------------------------------------------------------------------------

_STATE_HARDWARE = "hardware"
_STATE_THINKING = "thinking"
_STATE_ART = "art"

_COLOR_HEADER_THINKING = "#4fd1c5"
_COLOR_HEADER_ART      = "#27ae60"
_COLOR_HEADER_HW       = "#4fd1c5"


class ContextPane(Widget):
    """Right panel: THINKING / ART / HARDWARE state machine."""

    DEFAULT_CSS = """
    ContextPane {
        width: 40;
        layout: vertical;
    }
    #ctx-header {
        height: 2;
        padding: 0 1;
        color: #4fd1c5;
        background: #0f2a35;
    }
    #ctx-body {
        height: 1fr;
        padding: 0 1;
        overflow-y: auto;
    }
    """

    def __init__(self, vocabulary: frozenset[str]) -> None:
        super().__init__()
        self._vocabulary = vocabulary
        self._state = _STATE_HARDWARE
        self._token_buffer: list[str] = []  # accumulates colored Rich spans
        self._prev_token = ""
        self._is_sentence_start = True

    def compose(self) -> ComposeResult:
        yield Static("", id="ctx-header")
        yield Static("", id="ctx-body", markup=True)

    # ------------------------------------------------------------------
    # Public methods called by app handlers
    # ------------------------------------------------------------------

    def on_stream_start(self, task: str) -> None:
        """Transition to THINKING state, clear the body."""
        self._state = _STATE_THINKING
        self._token_buffer = []
        self._prev_token = ""
        self._is_sentence_start = True
        self._set_header(f"[{_COLOR_HEADER_THINKING}]THINKING  [{task}][/]")
        self._set_body("")

    def on_token(self, text: str) -> None:
        """Append a streamed token to the THINKING display."""
        if self._state != _STATE_THINKING:
            return
        for word in text.split(" "):
            if not word:
                self._token_buffer.append(" ")
                continue
            color = classify_token(
                word, self._vocabulary,
                prev_token=self._prev_token,
                is_sentence_start=self._is_sentence_start,
            )
            self._token_buffer.append(f"[{color}]{word}[/] ")
            self._prev_token = word.strip(".,!?;:'\"-()[]").lower()
            # Detect sentence boundaries — token ending in . ! ? starts a new sentence
            self._is_sentence_start = word.rstrip().endswith((".", "!", "?"))
        self._set_body("".join(self._token_buffer))

    def on_stream_done(self, task: str) -> None:
        """Transition out of THINKING state."""
        if task == "art":
            self._state = _STATE_ART
        else:
            self._state = _STATE_HARDWARE
            self._set_header(f"[{_COLOR_HEADER_HW}]HARDWARE[/]")

    def on_show_art(self, text: str, room_name: str) -> None:
        """Display cached ASCII art; transition to ART state."""
        self._state = _STATE_ART
        self._set_header(f"[{_COLOR_HEADER_ART}]{room_name.upper()}[/]")
        # Escape brackets in art so Rich doesn't misinterpret them
        safe = text.replace("[", "\\[")
        self._set_body(safe)

    def on_hardware_update(self, snapshot) -> None:
        """Refresh HARDWARE state display."""
        if self._state != _STATE_HARDWARE:
            return
        self._set_header(f"[{_COLOR_HEADER_HW}]HARDWARE[/]")
        self._set_body(_render_hardware(snapshot))

    # ------------------------------------------------------------------
    # Private helpers
    # ------------------------------------------------------------------

    def _set_header(self, text: str) -> None:
        self.query_one("#ctx-header", Static).update(text)

    def _set_body(self, text: str) -> None:
        self.query_one("#ctx-body", Static).update(text)


def _render_hardware(snap) -> str:
    """Render a HardwareSnapshot as Rich-marked-up text."""
    if all(
        getattr(snap, f) == -1
        for f in ("tensix_pct", "riscv_pct", "dram_read_gbps", "dram_write_gbps", "temp_c", "power_w")
    ):
        return "[#607d8b]pure Python — no hardware[/]"

    parts: list[str] = []

    if snap.tensix_pct != -1:
        bar = _progress_bar(snap.tensix_pct, "#27ae60")
        parts.append(f"[#4fd1c5]TENSIX[/]\n{bar}  {snap.tensix_pct:.0f}%\n")
    if snap.riscv_pct != -1:
        bar = _progress_bar(snap.riscv_pct, "#f4c471")
        parts.append(f"[#f4c471]RISC-V[/]\n{bar}  {snap.riscv_pct:.0f}%\n")
    if snap.dram_read_gbps != -1:
        parts.append(f"[#607d8b]DRAM[/]  [#4fd1c5]▸ {snap.dram_read_gbps:.1f} GB/s r[/]  [#81e6d9]▸ {snap.dram_write_gbps:.1f} GB/s w[/]\n")
    if snap.temp_c != -1:
        parts.append(f"[#607d8b]temp[/]  [#e8f0f2]{snap.temp_c:.0f}°C[/]")
    if snap.power_w != -1:
        parts.append(f"  [#607d8b]power[/]  [#e8f0f2]{snap.power_w:.0f}W[/]")

    return "".join(parts)


def _progress_bar(pct: float, color: str, width: int = 10) -> str:
    filled = int(pct / 100 * width)
    bar_on  = "█" * filled
    bar_off = "░" * (width - filled)
    return f"[{color}]{bar_on}[/][#2d3142]{bar_off}[/]"
```

- [ ] **Step 4: Run colorizer tests**

```bash
pytest tests/test_tui_colorizer.py -v
```

Expected: `10 passed`

- [ ] **Step 5: Run all tests to check for regressions**

```bash
pytest tests/ -v --ignore=tests/ttlang --ignore=tests/unit -x
```

Expected: all pass (no regressions)

- [ ] **Step 6: Commit**

```bash
git add tui/context_pane.py tests/test_tui_colorizer.py
git commit -m "feat: add tui/context_pane.py — THINKING/ART/HARDWARE state machine + colorizer"
```

---

## Task 7: `tui/app.py` — `ZMachineTuiApp`

**Files:**
- Create: `tui/app.py`

No unit tests for the full App — integration is covered in Task 9. The game thread logic is tested indirectly.

- [ ] **Step 1: Create `tui/app.py`**

```python
# tui/app.py
"""ZMachineTuiApp — Textual application wrapping the Z-machine game loop.

Layout (horizontal):
    GamePane (1fr)  │  ContextPane (40 cols fixed)

Below: Static status bar + Footer.

The game runs in a background executor thread (_run_game). All state changes
are posted to the Textual event loop via call_from_thread + post_message.

Custom messages (module-level, not nested):
    GameText      — game/remix output to display in GamePane
    TokenReceived — one streaming LLM token → ContextPane THINKING display
    StreamStart   — LLM call started (task: "art"|"remix")
    StreamDone    — LLM call finished (task, room_name)
    HardwareUpdate— fresh HardwareSnapshot from tt-smi poll
    ShowArt       — ASCII art ready after StreamDone("art")
"""
from __future__ import annotations

import asyncio
import queue
import threading
from pathlib import Path
from typing import Optional

from textual.app import App, ComposeResult
from textual.binding import Binding
from textual.message import Message
from textual.widgets import Footer, Input, Static
from textual.containers import Horizontal, Vertical

from tui.game_pane import GamePane
from tui.context_pane import ContextPane
from tui.hw_poller import poll as hw_poll, HardwareSnapshot
from tui.vocabulary import extract_vocabulary

# Lazy art prompt — read once from file, shared across all art calls
_ART_SYSTEM_PATH = Path(__file__).parent.parent / "prompts" / "ascii_system.txt"


# ---------------------------------------------------------------------------
# Custom messages
# ---------------------------------------------------------------------------

class GameText(Message):
    def __init__(self, content: str) -> None:
        super().__init__()
        self.content = content


class TokenReceived(Message):
    def __init__(self, text: str) -> None:
        super().__init__()
        self.text = text


class StreamStart(Message):
    def __init__(self, task: str) -> None:
        super().__init__()
        self.task = task


class StreamDone(Message):
    def __init__(self, task: str, room_name: str = "") -> None:
        super().__init__()
        self.task = task
        self.room_name = room_name


class HardwareUpdate(Message):
    def __init__(self, snapshot: HardwareSnapshot) -> None:
        super().__init__()
        self.snapshot = snapshot


class ShowArt(Message):
    def __init__(self, text: str, room_name: str) -> None:
        super().__init__()
        self.text = text
        self.room_name = room_name


# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------

from engines.sim import SimEngine  # noqa: E402 — used only for type annotation

_STAGE_LABELS = {
    "sim":    "Stage 1 — Pure Python",
    "device": "Stage 2 — QB2 DRAM",
    "risc-v": "Stage 3 — RISC-V cores",
}


class ZMachineTuiApp(App[None]):
    """Full-terminal TUI for any Z-machine V3 game.

    Args:
        engine:       Loaded BaseEngine instance.
        remix_layer:  RemixLayer or None (TUI works without remix).
        game_path:    Path to the .z3 file (used for vocabulary extraction).
    """

    CSS = """
    Screen {
        layout: vertical;
    }
    #main-row {
        layout: horizontal;
        height: 1fr;
    }
    GamePane {
        width: 1fr;
    }
    ContextPane {
        width: 40;
    }
    #status-bar {
        height: 1;
        padding: 0 1;
        color: #607d8b;
        background: #0f2a35;
    }
    """

    BINDINGS = [
        Binding("/", "focus_input", "Focus input", show=False),
        Binding("f1", "toggle_context", "Context", show=True),
        Binding("q", "action_quit_confirm", "Quit"),
    ]

    def __init__(self, engine, remix_layer=None, game_path: str = "game/zork1.z3") -> None:
        super().__init__()
        self._engine = engine
        self._remix_layer = remix_layer
        self._game_path = game_path
        self._input_queue: queue.Queue = queue.Queue()
        self._turn_count = 0
        self._stage = getattr(engine, "_stage", "sim")

        # Extract vocabulary for the colorizer
        try:
            from ttlang.zmachine_v3 import ZMachineV3
            zm = ZMachineV3(Path(game_path).read_bytes())
            self._vocabulary = extract_vocabulary(zm)
        except Exception:
            self._vocabulary = frozenset()

        # Read art system prompt once
        try:
            self._art_system = _ART_SYSTEM_PATH.read_text()
        except Exception:
            self._art_system = ""

    def compose(self) -> ComposeResult:
        with Horizontal(id="main-row"):
            yield GamePane(id="game")
            yield ContextPane(id="context", vocabulary=self._vocabulary)
        yield Static("", id="status-bar")
        yield Footer()

    def on_mount(self) -> None:
        self._update_status()
        # Start hardware polling
        self.set_interval(1.0, self._tick_hardware)
        # Start game thread
        self._game_thread = threading.Thread(target=self._run_game, daemon=True)
        self._game_thread.start()

    # ------------------------------------------------------------------
    # Hardware polling (runs on event loop via set_interval)
    # ------------------------------------------------------------------

    async def _tick_hardware(self) -> None:
        """Poll tt-smi in a thread pool so the event loop isn't blocked."""
        loop = asyncio.get_event_loop()
        snap = await loop.run_in_executor(None, hw_poll)
        snap.stage_label = _STAGE_LABELS.get(self._stage, self._stage)
        self.post_message(HardwareUpdate(snap))

    # ------------------------------------------------------------------
    # Message handlers — route to panes
    # ------------------------------------------------------------------

    def on_game_text(self, msg: GameText) -> None:
        self.query_one(GamePane).write_game_text(msg.content)

    def on_token_received(self, msg: TokenReceived) -> None:
        self.query_one(ContextPane).on_token(msg.text)

    def on_stream_start(self, msg: StreamStart) -> None:
        self.query_one(ContextPane).on_stream_start(msg.task)

    def on_stream_done(self, msg: StreamDone) -> None:
        ctx = self.query_one(ContextPane)
        ctx.on_stream_done(msg.task)

    def on_hardware_update(self, msg: HardwareUpdate) -> None:
        self.query_one(ContextPane).on_hardware_update(msg.snapshot)

    def on_show_art(self, msg: ShowArt) -> None:
        self.query_one(ContextPane).on_show_art(msg.text, msg.room_name)

    # ------------------------------------------------------------------
    # Input handling
    # ------------------------------------------------------------------

    def on_input_submitted(self, event: Input.Submitted) -> None:
        cmd = event.value.strip()
        if not cmd:
            return

        event.input.clear()

        if cmd.startswith("/"):
            handled, msg_text = self._handle_meta(cmd)
            if msg_text:
                self.query_one(GamePane).write_game_text(msg_text + "\n")
            if handled == "quit":
                self.exit()
            return

        event.input.disabled = True
        self._turn_count += 1
        self._input_queue.put(cmd)
        self._update_status()

    def _handle_meta(self, cmd: str):
        c = cmd.lower()
        if c == "/quit":
            return "quit", "[Goodbye!]"
        if c == "/help":
            return "", "Commands: /classic  /remix  /help  /quit"
        if c == "/remix" and self._remix_layer:
            self._remix_layer.active = True
            return "", "[Remix mode ON]"
        if c == "/classic" and self._remix_layer:
            self._remix_layer.active = False
            return "", "[Classic mode]"
        return "", f"[Unknown: {cmd}]"

    # ------------------------------------------------------------------
    # Game thread
    # ------------------------------------------------------------------

    def _run_game(self) -> None:
        """Background thread: drives the engine and posts events to Textual."""
        from remix.mode import _looks_like_room_entry, _extract_room_name
        from remix.llm import call_llm_stream
        from remix.router import route
        from remix.ascii_artist import _frame as _frame_art

        out = self._engine.startup()
        self.call_from_thread(self.post_message, GameText(out))
        self.call_from_thread(self._enable_input)

        while True:
            cmd = self._input_queue.get()  # blocks until player submits
            if cmd is None:
                break

            game_response = self._engine.step(cmd)

            if not self._remix_layer or not self._remix_layer.active:
                # Classic mode: show raw Z-machine output
                self.call_from_thread(self.post_message, GameText(game_response))
            else:
                # Remix mode: streaming art + streaming remix
                from remix.input_mapper import map_input
                mapped_cmd = map_input(cmd)

                if _looks_like_room_entry(game_response):
                    room_name = _extract_room_name(game_response)
                    key = room_name.strip().lower()

                    if key not in self._remix_layer._artist._cache and self._art_system:
                        # Stream art generation into THINKING panel
                        self.call_from_thread(self.post_message, StreamStart("art"))
                        art_tokens: list[str] = []
                        for tok in call_llm_stream(
                            system=self._art_system,
                            user=f"Room: {room_name}\nDescription: {game_response[:200]}",
                            model=route("art"),
                            temperature=0.9,
                        ):
                            art_tokens.append(tok)
                            self.call_from_thread(self.post_message, TokenReceived(tok))

                        raw_art = "".join(art_tokens).strip()
                        if raw_art:
                            framed = _frame_art(raw_art, room_name)
                        else:
                            framed = ""
                        self._remix_layer._artist._cache[key] = framed
                        self.call_from_thread(self.post_message, StreamDone("art", room_name=room_name))
                        if framed:
                            self.call_from_thread(self.post_message, ShowArt(framed, room_name))

                    elif key in self._remix_layer._artist._cache:
                        cached = self._remix_layer._artist._cache[key]
                        if cached:
                            self.call_from_thread(self.post_message, ShowArt(cached, room_name))

                # Stream remix output
                self.call_from_thread(self.post_message, StreamStart("remix"))
                from remix.output_remixer import remix_output_stream

                def _on_tok(t: str) -> None:
                    self.call_from_thread(self.post_message, TokenReceived(t))

                final_text = remix_output_stream(mapped_cmd, game_response, on_token=_on_tok)
                self.call_from_thread(self.post_message, StreamDone("remix"))
                self.call_from_thread(self.post_message, GameText(final_text))

            self.call_from_thread(self._enable_input)
            self.call_from_thread(self._update_status)

            if self._engine.game_over:
                break

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _enable_input(self) -> None:
        try:
            inp = self.query_one("#game-input", Input)
            inp.disabled = False
            inp.focus()
        except Exception:
            pass

    def _disable_input(self) -> None:
        try:
            self.query_one("#game-input", Input).disabled = True
        except Exception:
            pass

    def _update_status(self) -> None:
        from remix.llm import TT_INFERENCE_URL
        stage = _STAGE_LABELS.get(self._stage, self._stage)
        mode = "Remix" if (self._remix_layer and self._remix_layer.active) else "Classic"
        host = TT_INFERENCE_URL.split("//")[-1].split("/")[0]
        bar = f"{stage} · {mode} · {host} · {self._turn_count} commands"
        try:
            self.query_one("#status-bar", Static).update(bar)
        except Exception:
            pass

    # ------------------------------------------------------------------
    # Actions
    # ------------------------------------------------------------------

    def action_focus_input(self) -> None:
        try:
            self.query_one("#game-input", Input).focus()
        except Exception:
            pass

    def action_toggle_context(self) -> None:
        ctx = self.query_one(ContextPane)
        ctx.display = not ctx.display

    def action_quit_confirm(self) -> None:
        self._input_queue.put(None)  # sentinel to stop game thread
        self.exit()
```

- [ ] **Step 2: Run full test suite to check nothing is broken**

```bash
pytest tests/ -v --ignore=tests/ttlang --ignore=tests/unit -x
```

Expected: all previously passing tests still pass; no new failures.

- [ ] **Step 3: Commit**

```bash
git add tui/app.py
git commit -m "feat: add tui/app.py — ZMachineTuiApp with game thread, HW polling, streaming"
```

---

## Task 8: Wire `--tui` flag in `play.py`

**Files:**
- Modify: `play.py`

- [ ] **Step 1: Add the `--tui` argument and routing**

In `play.py`, add one argument to the parser in `main()` (after the `--persona` argument):

```python
parser.add_argument("--tui", action="store_true",
                    help="Launch Textual TUI (side panel with art, LLM stream, hardware)")
```

Add the Textual import guard and routing block in `main()` just before `game_loop(...)`:

```python
    engine = build_engine(args.stage, args.game)
    try:
        if args.tui:
            try:
                from tui.app import ZMachineTuiApp
            except ImportError:
                print("Error: 'textual' not installed. Run: pip install textual", file=sys.stderr)
                sys.exit(1)
            ZMachineTuiApp(
                engine=engine,
                remix_layer=remix_layer,
                game_path=args.game,
            ).run()
        else:
            game_loop(engine, remix_layer)
    finally:
        engine.close()
```

The final `main()` function should look like:

```python
def main() -> None:
    parser = argparse.ArgumentParser(description="Zork I on Tenstorrent")
    parser.add_argument("--stage", choices=["sim", "device", "risc-v"],
                        default="sim", help="Engine backend")
    parser.add_argument("--game", default="game/zork1.z3",
                        help="Path to Z-machine game file")
    parser.add_argument("--remix", action="store_true",
                        help="Enable LLM remix layer at startup")
    parser.add_argument("--model", default=None,
                        help="Override LLM model (default: qwen2.5:1.5b)")
    parser.add_argument("--persona", default=None,
                        help="Auto-play persona (expert/naive/completionist/experimental)")
    parser.add_argument("--tui", action="store_true",
                        help="Launch Textual TUI (side panel with art, LLM stream, hardware)")
    args = parser.parse_args()

    if not Path(args.game).exists():
        print(f"Error: game file not found: {args.game}", file=sys.stderr)
        sys.exit(1)

    print(BANNER)
    print(f"\n  {STAGE_LABELS.get(args.stage, args.stage)}\n")

    remix_layer = None
    if args.remix:
        from remix.mode import RemixLayer
        remix_layer = RemixLayer(model=args.model)
        print("  [Remix layer active — type /classic to disable]\n")

    engine = build_engine(args.stage, args.game)
    try:
        if args.tui:
            try:
                from tui.app import ZMachineTuiApp
            except ImportError:
                print("Error: 'textual' not installed. Run: pip install textual", file=sys.stderr)
                sys.exit(1)
            ZMachineTuiApp(
                engine=engine,
                remix_layer=remix_layer,
                game_path=args.game,
            ).run()
        else:
            game_loop(engine, remix_layer)
    finally:
        engine.close()
```

- [ ] **Step 2: Verify `--help` shows the new flag**

```bash
python play.py --help
```

Expected output includes: `--tui  Launch Textual TUI ...`

- [ ] **Step 3: Run full test suite**

```bash
pytest tests/ -v --ignore=tests/ttlang --ignore=tests/unit -x
```

Expected: all pass

- [ ] **Step 4: Commit**

```bash
git add play.py
git commit -m "feat: add --tui flag to play.py, route to ZMachineTuiApp"
```

---

## Task 9: Integration smoke test

**Files:**
- No new test files — use manual smoke test steps

- [ ] **Step 1: Stage 1 classic smoke test (no TUI, no regression)**

```bash
echo -e "open mailbox\nquit" | python play.py --stage sim game/zork1.z3
```

Expected: game shows opening text, "Opening the small mailbox reveals a leaflet.", `[Goodbye!]`. No errors.

- [ ] **Step 2: Verify `--tui` without textual shows helpful error (textual IS installed so skip)**

Since textual is already installed, verify the TUI launches and exits cleanly:

```bash
echo "quit" | python play.py --tui --stage sim game/zork1.z3 2>&1 | head -5
```

Expected: Textual app starts, receives "quit", exits cleanly (no Python tracebacks). Output may be empty or contain Textual escape sequences — what matters is the exit code is 0.

- [ ] **Step 3: Verify all test suites pass**

```bash
pytest tests/ -v --ignore=tests/ttlang --ignore=tests/unit
```

Expected: all 4 new test files pass, all existing tests pass. Count includes:
- `tests/test_llm_stream.py` — 7 tests
- `tests/test_tui_vocabulary.py` — 3 tests
- `tests/test_hw_poller.py` — 6 tests
- `tests/test_tui_colorizer.py` — 10 tests

- [ ] **Step 4: Final commit**

```bash
git add -u
git commit -m "feat: TUI status layer complete — --tui flag, streaming, colorizer, hw telemetry"
```

---

## Self-Review Checklist

**Spec coverage:**
- [x] §2 play.py `--tui` flag + `engine.close()` on exit via `finally`
- [x] §3 `call_llm_stream()` alongside existing `call_llm()` (no modification)
- [x] §4 `remix_output_stream()` alongside existing `remix_output()` (no modification)
- [x] §5 `tui/vocabulary.py` — Z-machine dict extraction, frozenset, graceful failure
- [x] §6 `tui/hw_poller.py` — HardwareSnapshot, poll(), never raises
- [x] §7 `tui/context_pane.py` — THINKING/ART/HARDWARE state machine + 5-rule colorizer
- [x] §8 `tui/game_pane.py` — RichLog + Input widget
- [x] §9 `tui/app.py` — layout CSS, game thread, `asyncio.Queue`-less design (stdlib `queue.Queue`), key bindings
- [x] §10 demo scripts not modified
- [x] §11 error handling: textual not installed → helpful message; tt-smi not found → -1 fields; stream empty → fallback
- [x] §12 all four test files

**No placeholders:** All steps contain exact code.

**Type consistency:** `classify_token(token, vocabulary, prev_token="", is_sentence_start=False)` used consistently in context_pane.py and tests.
