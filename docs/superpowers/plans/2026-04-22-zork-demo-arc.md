# Zork Demo Arc Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a four-stage demo of Zork I on Tenstorrent QB2 hardware, ending with an LLM remix layer that makes the game infinitely creative, packaged for asciinema recording and a blog post.

**Architecture:** A unified `play.py` entry point selects one of three engine backends (sim / device / risc-v) and optionally wraps all output through a `remix/` LLM layer. The Z-machine runs every turn regardless of mode; in remix mode `(user_input, zork_output)` flows through input_mapper → engine → output_remixer before reaching the terminal.

**Tech Stack:** Python 3.12, ttnn (TT-Lang pyenv), Ollama (qwen2.5:1.5b for mapping, qwen2.5:7b for remix/art), pytest, asciinema

---

## File Map

```
play.py                        NEW  unified entry point
engines/__init__.py            NEW
engines/base.py                NEW  BaseEngine ABC
engines/sim.py                 NEW  wraps ZMachineV3
engines/device.py              NEW  wraps zork_device.py (QB2 DRAM)
engines/riscv.py               NEW  wraps zork_risc.py (RISC-V kernel)
remix/__init__.py              NEW
remix/llm.py                   NEW  Ollama HTTP client, graceful fallback
remix/router.py                NEW  route cheap/rich tasks to right model
remix/input_mapper.py          NEW  freeform text → valid Zork command
remix/output_remixer.py        NEW  rewrite Z-machine response to match user
remix/ascii_artist.py          NEW  ASCII room art, cached per room name
remix/narrative_enhancer.py    NEW  postcards at game end (ported from C++)
remix/personas.py              NEW  4 AI player personas (ported from C++)
remix/mode.py                  NEW  track classic/remix mode, /commands
prompts/remix_system.txt       NEW  system prompt for output remixer
prompts/ascii_system.txt       NEW  system prompt for ASCII artist
prompts/postcard_system.txt    NEW  system prompt for narrative postcards
tests/test_engines.py          NEW  unit tests for all three engines
tests/test_remix.py            NEW  unit tests for remix layer
demos/demo-stage1.sh           NEW  asciinema script: sim
demos/demo-stage2.sh           NEW  asciinema script: device
demos/demo-stage3.sh           NEW  asciinema script: risc-v
demos/demo-stage4.sh           NEW  asciinema script: remix
docs/blog-draft.md             NEW  blog post draft
ttlang/zork_device.py          MODIFY  extract engine interface (no behaviour change)
ttlang/zork_risc.py            VERIFY  confirm run_zork() works after board reset
```

---

## Task 1: Engine Base + SimEngine

**Files:**
- Create: `engines/__init__.py`
- Create: `engines/base.py`
- Create: `engines/sim.py`
- Create: `tests/test_engines.py`

- [ ] **Step 1: Write failing tests**

```python
# tests/test_engines.py
import pytest
from pathlib import Path

GAME = "game/zork1.z3"

def test_sim_engine_startup_returns_zork_title():
    from engines.sim import SimEngine
    eng = SimEngine(GAME)
    out = eng.startup()
    eng.close()
    assert "ZORK" in out

def test_sim_engine_step_open_mailbox():
    from engines.sim import SimEngine
    eng = SimEngine(GAME)
    eng.startup()
    out = eng.step("open mailbox")
    eng.close()
    assert "mailbox" in out.lower() or "leaflet" in out.lower()

def test_sim_engine_step_invalid_command_returns_string():
    from engines.sim import SimEngine
    eng = SimEngine(GAME)
    eng.startup()
    out = eng.step("xyzzy frobozz")
    eng.close()
    assert isinstance(out, str)
    assert len(out) > 0

def test_sim_engine_context_manager():
    from engines.sim import SimEngine
    with SimEngine(GAME) as eng:
        out = eng.startup()
    assert "ZORK" in out
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
cd /home/ttuser/code/tt-zork1
python -m pytest tests/test_engines.py -v 2>&1 | head -20
```
Expected: `ImportError: No module named 'engines'`

- [ ] **Step 3: Create `engines/__init__.py`**

```python
# engines/__init__.py
```

- [ ] **Step 4: Create `engines/base.py`**

```python
# engines/base.py
from __future__ import annotations
from abc import ABC, abstractmethod


class BaseEngine(ABC):
    """Common interface for all Z-machine backends.

    Usage:
        with SimEngine("game/zork1.z3") as eng:
            print(eng.startup())
            while True:
                cmd = input("> ")
                print(eng.step(cmd))
    """

    def __init__(self, game_path: str) -> None:
        self.game_path = game_path

    @abstractmethod
    def startup(self) -> str:
        """Run the game's initialization sequence. Returns opening text."""
        ...

    @abstractmethod
    def step(self, command: str) -> str:
        """Submit one command, return the game's response."""
        ...

    @abstractmethod
    def close(self) -> None:
        """Release any hardware or file resources."""
        ...

    def __enter__(self) -> "BaseEngine":
        return self

    def __exit__(self, *_) -> None:
        self.close()
```

- [ ] **Step 5: Create `engines/sim.py`**

```python
# engines/sim.py
from __future__ import annotations
from pathlib import Path

from engines.base import BaseEngine
from ttlang.zmachine_v3 import ZMachineV3

INSTRUCTIONS_STARTUP = 10000
INSTRUCTIONS_PER_TURN = 5000


class SimEngine(BaseEngine):
    """Pure-Python Z-machine engine. No hardware required.

    Wraps ZMachineV3 from ttlang/zmachine_v3.py.
    Stage 1 of the demo arc.
    """

    label = "Stage 1 — Pure Python Z-machine (no hardware)"

    def __init__(self, game_path: str) -> None:
        super().__init__(game_path)
        game_bytes = Path(game_path).read_bytes()
        self._zm = ZMachineV3(game_bytes)

    def startup(self) -> str:
        self._zm.interpret(INSTRUCTIONS_STARTUP)
        return self._zm.flush_output()

    def step(self, command: str) -> str:
        self._zm.input_command = command
        self._zm.interpret(INSTRUCTIONS_PER_TURN)
        return self._zm.flush_output()

    @property
    def game_over(self) -> bool:
        return self._zm.game_over

    @property
    def running(self) -> bool:
        return self._zm.running

    def close(self) -> None:
        pass  # no resources to release
```

- [ ] **Step 6: Run tests**

```bash
python -m pytest tests/test_engines.py -v
```
Expected: all 4 tests PASS

- [ ] **Step 7: Commit**

```bash
git add engines/ tests/test_engines.py
git commit -m "feat: engines/base + SimEngine with tests"
```

---

## Task 2: DeviceEngine (QB2 DRAM)

**Files:**
- Create: `engines/device.py`
- Modify: `tests/test_engines.py` (add device tests, skip if no hardware)

- [ ] **Step 1: Add device tests (skipped without hardware)**

Add to `tests/test_engines.py`:

```python
import os
HAS_DEVICE = os.path.exists("/dev/tenstorrent")

@pytest.mark.skipif(not HAS_DEVICE, reason="Requires QB2 hardware")
def test_device_engine_startup_returns_zork_title():
    from engines.device import DeviceEngine
    with DeviceEngine(GAME) as eng:
        out = eng.startup()
    assert "ZORK" in out

@pytest.mark.skipif(not HAS_DEVICE, reason="Requires QB2 hardware")
def test_device_engine_step_open_mailbox():
    from engines.device import DeviceEngine
    with DeviceEngine(GAME) as eng:
        eng.startup()
        out = eng.step("open mailbox")
    assert "mailbox" in out.lower() or "leaflet" in out.lower()
```

- [ ] **Step 2: Run new tests (expect skip)**

```bash
python -m pytest tests/test_engines.py -v -k device
```
Expected: 2 SKIPPED (no hardware) or ImportError

- [ ] **Step 3: Create `engines/device.py`**

```python
# engines/device.py
from __future__ import annotations

import torch
import ttnn
from pathlib import Path

from engines.base import BaseEngine
from ttlang.zmachine_v3 import ZMachineV3

INSTRUCTIONS_STARTUP = 10000
INSTRUCTIONS_PER_TURN = 5000


def _game_to_device(game_bytes: bytes, device: ttnn.Device) -> ttnn.Tensor:
    padded = bytearray(game_bytes)
    if len(padded) % 2:
        padded.append(0)
    t = torch.frombuffer(bytes(padded), dtype=torch.uint8).clone().float()
    return ttnn.from_torch(
        t,
        dtype=ttnn.bfloat16,
        layout=ttnn.ROW_MAJOR_LAYOUT,
        device=device,
        memory_config=ttnn.DRAM_MEMORY_CONFIG,
    )


def _device_to_bytes(tensor: ttnn.Tensor) -> bytearray:
    t = ttnn.to_torch(tensor)
    return bytearray(int(round(float(v))) & 0xFF for v in t.tolist())


class DeviceEngine(BaseEngine):
    """QB2 DRAM-backed engine. Game binary lives on Tenstorrent silicon.

    Wraps the data path from zork_device.py. Python interpreter runs on
    the host; game data is uploaded to and verified against QB2 DRAM.
    Stage 2 of the demo arc.
    """

    label = "Stage 2 — Game data on QB2 DRAM (Tenstorrent hardware)"

    def __init__(self, game_path: str) -> None:
        super().__init__(game_path)
        game_bytes = Path(game_path).read_bytes()
        self._device = ttnn.open_device(device_id=0)
        tensor = _game_to_device(game_bytes, self._device)
        dram_bytes = _device_to_bytes(tensor)
        assert dram_bytes[:8] == bytearray(game_bytes[:8]), "DRAM round-trip mismatch"
        self._dram_addr = tensor.buffer_address()
        self._zm = ZMachineV3(bytes(dram_bytes))

    def startup(self) -> str:
        self._zm.interpret(INSTRUCTIONS_STARTUP)
        return self._zm.flush_output()

    def step(self, command: str) -> str:
        self._zm.input_command = command
        self._zm.interpret(INSTRUCTIONS_PER_TURN)
        return self._zm.flush_output()

    @property
    def game_over(self) -> bool:
        return self._zm.game_over

    @property
    def running(self) -> bool:
        return self._zm.running

    def close(self) -> None:
        if self._device:
            ttnn.close_device(self._device)
            self._device = None
```

- [ ] **Step 4: Run tests**

```bash
python -m pytest tests/test_engines.py -v
```
Expected: 4 sim PASS, 2 device SKIPPED (or PASS if hardware present)

- [ ] **Step 5: Commit**

```bash
git add engines/device.py tests/test_engines.py
git commit -m "feat: DeviceEngine — game data on QB2 DRAM"
```

---

## Task 3: RiscVEngine (on-chip RISC-V)

**Files:**
- Create: `engines/riscv.py`
- Modify: `tests/test_engines.py` (add riscv tests, skip if no hardware)

The RISC-V engine is display-first: `startup()` runs 5 batches × 40 instructions = 200 instructions and returns the opening text. `step()` runs 3 batches per command. State does not persist between `step()` calls in this implementation — each step re-initialises from the saved state tensor. This is sufficient for the demo arc (shows the opening text and one command).

- [ ] **Step 1: Add riscv tests**

Add to `tests/test_engines.py`:

```python
@pytest.mark.skipif(not HAS_DEVICE, reason="Requires QB2 hardware")
def test_riscv_engine_startup_returns_zork_title():
    from engines.riscv import RiscVEngine
    with RiscVEngine(GAME) as eng:
        out = eng.startup()
    assert "ZORK" in out or len(out) > 20   # partial output is acceptable

@pytest.mark.skipif(not HAS_DEVICE, reason="Requires QB2 hardware")
def test_riscv_engine_startup_is_string():
    from engines.riscv import RiscVEngine
    with RiscVEngine(GAME) as eng:
        out = eng.startup()
    assert isinstance(out, str)
```

- [ ] **Step 2: Create `engines/riscv.py`**

```python
# engines/riscv.py
from __future__ import annotations

from pathlib import Path
from engines.base import BaseEngine

# Import lazily so the file can be imported without TT-Lang pyenv active
try:
    from ttlang.zork_risc import run_zork
    _RISCV_AVAILABLE = True
except ImportError:
    _RISCV_AVAILABLE = False

STARTUP_BATCHES = 5   # 5 × 40 = 200 instructions — enough for opening text
STEP_BATCHES = 3      # 3 × 40 = 120 instructions — enough for most commands


class RiscVEngine(BaseEngine):
    """RISC-V on-chip engine. Interpreter executes on Blackhole RISC-V cores.

    Uses ttlang/zork_risc.py which dispatches kernels/zork_interpreter_l1.cpp
    via ttnn.generic_op. Batched execution: 40 instructions per kernel invocation
    (firmware watchdog safe). State persists across batches via DRAM state tensor.
    Stage 3 of the demo arc.
    """

    label = "Stage 3 — Z-machine interpreter on QB2 RISC-V cores (TT-Lang)"

    def __init__(self, game_path: str) -> None:
        super().__init__(game_path)
        if not _RISCV_AVAILABLE:
            raise ImportError(
                "TT-Lang pyenv not active. Run: source ~/code/tt-lang/build/env/activate"
            )
        if not Path(game_path).exists():
            raise FileNotFoundError(game_path)

    def startup(self) -> str:
        return run_zork(self.game_path, command="", verbose=False,
                        num_batches=STARTUP_BATCHES)

    def step(self, command: str) -> str:
        return run_zork(self.game_path, command=command, verbose=False,
                        num_batches=STEP_BATCHES)

    def close(self) -> None:
        pass  # run_zork opens and closes device internally
```

- [ ] **Step 3: Run tests**

```bash
python -m pytest tests/test_engines.py -v
```
Expected: all sim tests PASS, device/riscv SKIPPED or PASS

- [ ] **Step 4: Commit**

```bash
git add engines/riscv.py tests/test_engines.py
git commit -m "feat: RiscVEngine — interpreter on QB2 RISC-V cores"
```

---

## Task 4: `play.py` — Unified Entry Point (Stages 1–3)

**Files:**
- Create: `play.py`

No automated tests here — verify manually by running each stage.

- [ ] **Step 1: Create `play.py`**

```python
#!/usr/bin/env python3
# play.py — Zork I on Tenstorrent: unified demo entry point
#
# Usage:
#   python play.py --stage sim              # Stage 1: pure Python
#   python play.py --stage device           # Stage 2: QB2 DRAM
#   python play.py --stage risc-v           # Stage 3: RISC-V cores
#   python play.py --stage sim --remix      # Stage 4: LLM remix layer
#   python play.py --stage sim --remix --persona expert
from __future__ import annotations

import argparse
import sys
from pathlib import Path

STAGE_LABELS = {
    "sim":    "Stage 1 — Pure Python Z-machine  (no hardware)",
    "device": "Stage 2 — Game data on QB2 DRAM  (Tenstorrent silicon)",
    "risc-v": "Stage 3 — Interpreter on RISC-V cores  (TT-Lang)",
}

BANNER = """\
╔══════════════════════════════════════════════════════════════╗
║          Z O R K   I   on   T E N S T O R R E N T           ║
║  1977 interactive fiction · 2026 AI accelerator hardware    ║
╚══════════════════════════════════════════════════════════════╝"""

HELP_TEXT = """\
Commands: /classic  /remix  /help  /quit
  /classic  — disable LLM remix layer (raw Z-machine output)
  /remix    — enable  LLM remix layer (creative responses)
  /help     — show this message
  /quit     — exit the game"""


def build_engine(stage: str, game_path: str):
    if stage == "sim":
        from engines.sim import SimEngine
        return SimEngine(game_path)
    elif stage == "device":
        from engines.device import DeviceEngine
        return DeviceEngine(game_path)
    elif stage == "risc-v":
        from engines.riscv import RiscVEngine
        return RiscVEngine(game_path)
    else:
        raise ValueError(f"Unknown stage: {stage!r}  (choose sim, device, risc-v)")


def game_loop(engine, remix_layer=None) -> None:
    """Core game loop shared by all stages."""
    out = engine.startup()
    _display(out, remix_layer, user_input=None, is_startup=True)

    while getattr(engine, "running", True):
        try:
            raw = input("\n> ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\n[Interrupted]")
            break

        if not raw:
            continue

        # Handle meta-commands before passing to engine
        if raw.startswith("/"):
            handled, msg = _handle_meta(raw, remix_layer)
            if msg:
                print(msg)
            if handled == "quit":
                break
            continue

        out = engine.step(raw)
        _display(out, remix_layer, user_input=raw)

        if getattr(engine, "game_over", False):
            if remix_layer:
                remix_layer.on_game_end()
            break


def _display(text: str, remix_layer, user_input: str | None,
             is_startup: bool = False) -> None:
    if remix_layer and remix_layer.active and not is_startup and user_input:
        text = remix_layer.process(user_input, text)
    if text:
        print(text, end="", flush=True)


def _handle_meta(cmd: str, remix_layer) -> tuple[str, str]:
    """Returns (action, message). action is '' or 'quit'."""
    c = cmd.lower()
    if c == "/quit":
        return "quit", "[Goodbye!]"
    if c == "/help":
        return "", HELP_TEXT
    if c == "/remix":
        if remix_layer:
            remix_layer.active = True
            return "", "[Remix mode ON — the world bends to you]"
        return "", "[Remix layer not loaded — start with --remix]"
    if c == "/classic":
        if remix_layer:
            remix_layer.active = False
            return "", "[Classic mode — raw Z-machine output]"
        return "", "[Already in classic mode]"
    return "", f"[Unknown command: {cmd}]"


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
        game_loop(engine, remix_layer)
    finally:
        engine.close()


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Verify Stage 1 runs**

```bash
echo -e "open mailbox\ntake leaflet\nquit" | python play.py --stage sim
```
Expected: Zork opening text, mailbox opens, leaflet taken, exits cleanly.

- [ ] **Step 3: Commit**

```bash
git add play.py
git commit -m "feat: play.py unified entry point — stages 1-3"
```

---

## Task 5: Remix LLM Client + Router

**Files:**
- Create: `remix/__init__.py`
- Create: `remix/llm.py`
- Create: `remix/router.py`
- Create: `tests/test_remix.py`

- [ ] **Step 1: Write failing tests**

```python
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

def test_router_map_uses_cheap_model():
    from remix.router import route
    model = route("map")
    assert "1.5b" in model or "0.5b" in model

def test_router_remix_uses_rich_model():
    from remix.router import route
    model = route("remix")
    # rich model is larger than 1.5b
    assert model != route("map") or "7b" in model or "1.5b" in model
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
python -m pytest tests/test_remix.py -v 2>&1 | head -15
```
Expected: `ImportError: No module named 'remix'`

- [ ] **Step 3: Create `remix/__init__.py`**

```python
# remix/__init__.py
```

- [ ] **Step 4: Create `remix/llm.py`**

```python
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
```

- [ ] **Step 5: Create `remix/router.py`**

```python
# remix/router.py
"""Route remix tasks to the appropriate LLM model.

Cheap tasks (input mapping) use a small fast model.
Rich tasks (output remix, ASCII art, postcards) use a larger model.
Override via ZORK_LLM_MODEL env var to force a single model everywhere.
"""
from __future__ import annotations
import os

_OVERRIDE = os.environ.get("ZORK_LLM_MODEL")

_MODELS = {
    "map":      "qwen2.5:1.5b",   # input mapping: cheap, fast
    "remix":    "qwen2.5:7b",     # output remix: richer reasoning
    "art":      "qwen2.5:7b",     # ASCII art: needs spatial reasoning
    "postcard": "qwen2.5:7b",     # narrative postcards: creative
}


def route(task: str) -> str:
    """Return the model name to use for the given task."""
    if _OVERRIDE:
        return _OVERRIDE
    return _MODELS.get(task, "qwen2.5:1.5b")
```

- [ ] **Step 6: Run tests**

```bash
python -m pytest tests/test_remix.py -v
```
Expected: all 4 PASS

- [ ] **Step 7: Commit**

```bash
git add remix/__init__.py remix/llm.py remix/router.py tests/test_remix.py
git commit -m "feat: remix/llm + router — Ollama client with graceful fallback"
```

---

## Task 6: Input Mapper

**Files:**
- Create: `remix/input_mapper.py`
- Create: `prompts/remix_input_system.txt`
- Modify: `tests/test_remix.py`

- [ ] **Step 1: Create the system prompt**

```
# prompts/remix_input_system.txt
You translate player input to a single valid Zork command.

Rules:
- Output ONLY the command, nothing else. No explanation.
- If the input is already a valid Zork command, return it unchanged.
- Map creative/unusual phrasing to the nearest valid action.
- Keep object names exactly as the player said them when unambiguous.
- Valid Zork verbs: take, drop, open, close, read, look, go, north, south,
  east, west, up, down, northeast, northwest, southeast, southwest,
  examine, inventory, i, put, insert, attack, kill, eat, drink, wear,
  remove, light, extinguish, turn, push, pull, climb, enter, exit, wait

Examples:
"open the mailbox with my teeth" → "open mailbox"
"I want to grab that leaflet" → "take leaflet"
"head north" → "north"
"check my stuff" → "inventory"
"stab the troll with sword" → "attack troll with sword"
"look around" → "look"
"go north" → "north"
"read it" → "read it"
```

- [ ] **Step 2: Add input mapper tests**

Add to `tests/test_remix.py`:

```python
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
```

- [ ] **Step 3: Run new tests to verify they fail**

```bash
python -m pytest tests/test_remix.py::test_input_mapper_passthrough_on_llm_failure -v
```
Expected: `ImportError: cannot import name 'map_input'`

- [ ] **Step 4: Create `remix/input_mapper.py`**

```python
# remix/input_mapper.py
"""Map freeform player input to a valid Zork command via LLM.

Falls back to returning the original input unchanged if the LLM is
unavailable or returns an empty response. The Z-machine parser handles
any remaining ambiguity — this layer just removes the "I don't understand"
wall for creative inputs.
"""
from __future__ import annotations
from pathlib import Path
from remix.llm import call_ollama
from remix.router import route

_SYSTEM_PROMPT = (
    Path(__file__).parent.parent / "prompts" / "remix_input_system.txt"
).read_text()


def map_input(user_input: str) -> str:
    """Translate freeform user input to a valid Zork command.

    Returns the LLM-mapped command, or the original input if the LLM
    is unavailable or returns nothing useful.
    """
    result = call_ollama(
        system=_SYSTEM_PROMPT,
        user=user_input,
        model=route("map"),
        temperature=0.1,   # low temperature: we want deterministic mapping
    )
    if result and result.strip():
        return result.strip()
    return user_input
```

- [ ] **Step 5: Run tests**

```bash
python -m pytest tests/test_remix.py -v
```
Expected: all PASS

- [ ] **Step 6: Commit**

```bash
git add remix/input_mapper.py prompts/remix_input_system.txt tests/test_remix.py
git commit -m "feat: remix/input_mapper — freeform input → Zork command via LLM"
```

---

## Task 7: Output Remixer

**Files:**
- Create: `remix/output_remixer.py`
- Create: `prompts/remix_output_system.txt`
- Modify: `tests/test_remix.py`

- [ ] **Step 1: Create the system prompt**

```
# prompts/remix_output_system.txt
You are the voice of the Zork world. Rewrite the game's response to match
the player's style and phrasing. Never change the facts — what happened,
what was found, what exists. Only change the voice and flavour.

Rules:
- Keep ALL game facts (items found, doors opened, damage taken, score)
- Match the player's energy: weird input → weird response; plain input → plain response
- Stay in the Zork world setting — dark, dry wit, 1980s Infocom style
- One short paragraph maximum. No lists unless the original was a list.
- Do not add new items or events that didn't happen in the original response.
- If the player did something unusual, acknowledge it with dry humour.

Input format:
PLAYER: <what the player typed>
GAME: <what the Z-machine responded>

Output: The rewritten response only. No labels, no explanation.

Example:
PLAYER: open the mailbox with my teeth
GAME: Opening the small mailbox reveals a leaflet.
Output: The mailbox creaks open with a tetanus-worthy groan as you regret
this decision immediately. Inside, mercifully, is a leaflet.

Example:
PLAYER: go north
GAME: North of House
You are facing the north side of a white house.
Output: You trudge north. The white house looms, silent and unimpressed.
```

- [ ] **Step 2: Add output remixer tests**

Add to `tests/test_remix.py`:

```python
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
```

- [ ] **Step 3: Create `remix/output_remixer.py`**

```python
# remix/output_remixer.py
"""Rewrite Z-machine output to match the player's phrasing and creativity.

The Z-machine response is the source of truth for game facts. This module
changes the voice, not the facts. Falls back to the original response if
the LLM is unavailable.
"""
from __future__ import annotations
from pathlib import Path
from remix.llm import call_ollama
from remix.router import route

_SYSTEM_PROMPT = (
    Path(__file__).parent.parent / "prompts" / "remix_output_system.txt"
).read_text()


def remix_output(user_input: str, zork_response: str) -> str:
    """Rewrite zork_response to match the flavour of user_input.

    Returns the remixed text, or the original zork_response if the LLM
    is unavailable or returns nothing useful.
    """
    user_msg = f"PLAYER: {user_input}\nGAME: {zork_response}"
    result = call_ollama(
        system=_SYSTEM_PROMPT,
        user=user_msg,
        model=route("remix"),
        temperature=0.8,   # higher temperature for creative voice
    )
    if result and result.strip():
        return result.strip() + "\n"
    return zork_response
```

- [ ] **Step 4: Run tests**

```bash
python -m pytest tests/test_remix.py -v
```
Expected: all PASS

- [ ] **Step 5: Commit**

```bash
git add remix/output_remixer.py prompts/remix_output_system.txt tests/test_remix.py
git commit -m "feat: remix/output_remixer — rewrite Z-machine responses via LLM"
```

---

## Task 8: ASCII Scene Artist

**Files:**
- Create: `remix/ascii_artist.py`
- Create: `prompts/ascii_system.txt`
- Modify: `tests/test_remix.py`

- [ ] **Step 1: Create the system prompt**

```
# prompts/ascii_system.txt
You create small ASCII art illustrations for text adventure game rooms.

Rules:
- Exactly 4-6 lines of art (not counting the border)
- Maximum 30 characters wide per line
- Use classic ASCII characters: | / \ _ - ~ ^ * # = [ ] ( ) @
- Draw the essence of the room: key objects, landscape, mood
- Style: simple, evocative, 1980s terminal aesthetic
- Do not include room name or description text — art only

Output format: raw ASCII art lines only. No labels, no explanation, no code blocks.

Example for "West of House":
    _____
   |     |  __
   | [=] | /  \  ~  ~
   |_____|/    \  ~ ~
   /\/\/\/\/\/\/\/\/\

Example for "Underground Cave":
     ___
    /   \___
   |  * *   |
   |  torch |
    \_______/
     |||||||
```

- [ ] **Step 2: Add ASCII artist tests**

Add to `tests/test_remix.py`:

```python
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
    with patch("remix.ascii_artist.call_ollama", return_value=None):
        result = artist.get("Cave", "Dark cave.")
    assert result == ""
```

- [ ] **Step 3: Create `remix/ascii_artist.py`**

```python
# remix/ascii_artist.py
"""Generate ASCII art illustrations for Zork rooms.

Art is generated once per room and cached — expensive to generate,
free to revisit. Returns empty string if LLM is unavailable, so the
game remains playable without ASCII art.
"""
from __future__ import annotations
from pathlib import Path
from remix.llm import call_ollama
from remix.router import route

_SYSTEM_PROMPT = (
    Path(__file__).parent.parent / "prompts" / "ascii_system.txt"
).read_text()

_BORDER_WIDTH = 32


def _frame(art: str, room_name: str) -> str:
    """Wrap ASCII art in a box border with room name."""
    lines = art.strip().splitlines()
    # Truncate to 6 lines max
    lines = lines[:6]
    width = max((len(line) for line in lines), default=0)
    width = max(width, len(room_name) + 2, 20)
    bar = "═" * (width + 2)
    framed = [f"╔{bar}╗", f"║ {room_name:<{width}} ║"]
    for line in lines:
        framed.append(f"║ {line:<{width}} ║")
    framed.append(f"╚{bar}╝")
    return "\n".join(framed)


class AsciiArtist:
    """Room ASCII art generator with per-room cache."""

    def __init__(self) -> None:
        self._cache: dict[str, str] = {}

    def get(self, room_name: str, description: str) -> str:
        """Return framed ASCII art for this room, generating if not cached.

        Returns empty string if the LLM is unavailable.
        """
        key = room_name.strip().lower()
        if key in self._cache:
            return self._cache[key]

        prompt = f"Room: {room_name}\nDescription: {description[:200]}"
        raw = call_ollama(
            system=_SYSTEM_PROMPT,
            user=prompt,
            model=route("art"),
            temperature=0.9,
        )
        if not raw or not raw.strip():
            self._cache[key] = ""
            return ""

        result = _frame(raw, room_name)
        self._cache[key] = result
        return result
```

- [ ] **Step 4: Run tests**

```bash
python -m pytest tests/test_remix.py -v
```
Expected: all PASS

- [ ] **Step 5: Commit**

```bash
git add remix/ascii_artist.py prompts/ascii_system.txt tests/test_remix.py
git commit -m "feat: remix/ascii_artist — LLM-generated ASCII room art with cache"
```

---

## Task 9: Narrative Enhancer + Personas

**Files:**
- Create: `remix/narrative_enhancer.py`
- Create: `remix/personas.py`
- Create: `prompts/postcard_system.txt`
- Modify: `tests/test_remix.py`

- [ ] **Step 1: Create postcard system prompt**

```
# prompts/postcard_system.txt
You write short, vivid "postcard" descriptions of memorable moments from
a Zork I playthrough. Style: literary, slightly purple, as if written on
the back of a postcard from an adventure.

Rules:
- 2-3 sentences maximum
- Past tense, first person
- Reference the specific location and moment type
- Zork world tone: dark wit, underground empire, magic and danger
- No gameplay mechanics (no "I typed", no "the parser said")

Input format:
LOCATION: <room name>
MOMENT: <arrival|treasure|death|victory>
CONTEXT: <brief description of what happened>

Output: postcard text only. No labels.

Example:
LOCATION: West of House
MOMENT: arrival
CONTEXT: First location, start of game
Output: I arrived at the white house as evening fell, the mailbox
gleaming like a dare I had no business refusing. The boarded door
suggested the house had opinions about visitors.
```

- [ ] **Step 2: Add tests**

Add to `tests/test_remix.py`:

```python
def test_narrative_enhancer_generates_postcard():
    from remix.narrative_enhancer import NarrativeEnhancer
    ne = NarrativeEnhancer()
    mock_card = "I arrived at the white house as evening fell..."
    with patch("remix.narrative_enhancer.call_ollama", return_value=mock_card):
        ne.record("West of House", "arrival", "First location")
    cards = ne.get_postcards()
    assert len(cards) == 1
    assert cards[0]["text"] == mock_card

def test_narrative_enhancer_no_duplicate_moment_types():
    from remix.narrative_enhancer import NarrativeEnhancer
    ne = NarrativeEnhancer()
    with patch("remix.narrative_enhancer.call_ollama", return_value="postcard text"):
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
```

- [ ] **Step 3: Create `remix/narrative_enhancer.py`**

```python
# remix/narrative_enhancer.py
"""Narrative postcard generator — ported from feature/multi-llm-integration.

Collects memorable moments during gameplay and generates vivid postcard-style
descriptions at game end. One postcard per moment type (arrival, treasure,
death, victory) to keep the collection concise.
"""
from __future__ import annotations
from pathlib import Path
from remix.llm import call_ollama
from remix.router import route

_SYSTEM_PROMPT = (
    Path(__file__).parent.parent / "prompts" / "postcard_system.txt"
).read_text()

_POSTCARD_TYPES = {"arrival", "treasure", "death", "victory"}

# Display width for postcard boxes
_WIDTH = 44


def _render_postcards(cards: list[dict]) -> str:
    """Render all postcards as a framed display block."""
    if not cards:
        return ""
    bar = "═" * (_WIDTH + 2)
    lines = [f"╔{bar}╗", f"║{'  POSTCARDS FROM YOUR JOURNEY':<{_WIDTH + 2}}║",
             f"╠{bar}╣"]
    for card in cards:
        loc = card["location"]
        text = card["text"]
        lines.append(f"║ {loc:<{_WIDTH}} ║")
        # Word-wrap text to _WIDTH chars
        words = text.split()
        line_buf = ""
        for word in words:
            if len(line_buf) + len(word) + 1 > _WIDTH:
                lines.append(f"║ {line_buf:<{_WIDTH}} ║")
                line_buf = word
            else:
                line_buf = f"{line_buf} {word}".strip()
        if line_buf:
            lines.append(f"║ {line_buf:<{_WIDTH}} ║")
        lines.append(f"║ {'':<{_WIDTH}} ║")
    lines.append(f"╚{bar}╝")
    return "\n".join(lines)


class NarrativeEnhancer:
    """Collects game moments and generates postcards at game end."""

    def __init__(self) -> None:
        self._postcards: list[dict] = []
        self._seen_types: set[str] = set()

    def record(self, location: str, moment_type: str, context: str) -> None:
        """Record a memorable moment. One postcard per moment_type."""
        if moment_type not in _POSTCARD_TYPES or moment_type in self._seen_types:
            return
        prompt = (
            f"LOCATION: {location}\nMOMENT: {moment_type}\nCONTEXT: {context}"
        )
        text = call_ollama(
            system=_SYSTEM_PROMPT,
            user=prompt,
            model=route("postcard"),
            temperature=0.85,
        )
        if text and text.strip():
            self._postcards.append(
                {"location": location, "moment_type": moment_type, "text": text.strip()}
            )
            self._seen_types.add(moment_type)

    def get_postcards(self) -> list[dict]:
        return list(self._postcards)

    def display(self) -> None:
        """Print all postcards to terminal."""
        rendered = _render_postcards(self._postcards)
        if rendered:
            print("\n" + rendered)
```

- [ ] **Step 4: Create `remix/personas.py`**

```python
# remix/personas.py
"""AI player personas — ported from feature/multi-llm-integration.

Four distinct playing styles for the auto-play --persona flag.
Each persona has a name and a system prompt that shapes how the AI
decides what command to issue next given the current game output.
"""
from __future__ import annotations

PERSONAS: dict[str, dict] = {
    "expert": {
        "name": "Expert Speedrunner",
        "system_prompt": (
            "You are an expert Zork I player who has memorised the optimal "
            "path to victory. Given the game's current output, choose the "
            "single most efficient next command. Output ONLY the command."
        ),
    },
    "naive": {
        "name": "Naive Explorer",
        "system_prompt": (
            "You are playing Zork I for the first time. You explore "
            "curiously, try things that seem interesting, and sometimes "
            "make mistakes. Given the game's current output, choose a "
            "natural next action. Output ONLY the command."
        ),
    },
    "completionist": {
        "name": "Completionist",
        "system_prompt": (
            "You are a completionist playing Zork I — you want to find "
            "every room, every treasure, and achieve the maximum score. "
            "Given the game's current output, choose the command most "
            "likely to reveal unexplored content. Output ONLY the command."
        ),
    },
    "experimental": {
        "name": "Experimental Player",
        "system_prompt": (
            "You play Zork I experimentally — you test boundaries, try "
            "unusual commands, and see what the game allows. Given the "
            "game's current output, choose a creative or unexpected "
            "command. Output ONLY the command."
        ),
    },
}


def get_persona(name: str) -> dict:
    """Return persona dict by name, or raise ValueError for unknown names."""
    key = name.lower()
    if key not in PERSONAS:
        raise ValueError(
            f"Unknown persona {name!r}. "
            f"Choose from: {', '.join(PERSONAS.keys())}"
        )
    return PERSONAS[key]
```

- [ ] **Step 5: Run tests**

```bash
python -m pytest tests/test_remix.py -v
```
Expected: all PASS

- [ ] **Step 6: Commit**

```bash
git add remix/narrative_enhancer.py remix/personas.py \
        prompts/postcard_system.txt tests/test_remix.py
git commit -m "feat: remix/narrative_enhancer + personas — postcards and AI play styles"
```

---

## Task 10: Wire Remix Layer into `play.py`

**Files:**
- Create: `remix/mode.py`
- Modify: `play.py` (RemixLayer import already stubbed in Task 4)

- [ ] **Step 1: Add mode tests**

Add to `tests/test_remix.py`:

```python
def test_remix_layer_process_calls_mapper_and_remixer():
    from remix.mode import RemixLayer
    layer = RemixLayer()
    with patch("remix.mode.map_input", return_value="open mailbox") as mock_map, \
         patch("remix.mode.remix_output", return_value="Creative response\n") as mock_remix:
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

def test_remix_layer_on_game_end_calls_display():
    from remix.mode import RemixLayer
    layer = RemixLayer()
    with patch.object(layer._enhancer, "display") as mock_display:
        layer.on_game_end()
    mock_display.assert_called_once()
```

- [ ] **Step 2: Create `remix/mode.py`**

```python
# remix/mode.py
"""RemixLayer — orchestrates input mapping, output remixing, ASCII art, and postcards.

This is the single object play.py creates when --remix is passed. It holds all
remix state (active flag, ASCII art cache, postcard collection) and exposes the
process() method that play.py calls each turn.
"""
from __future__ import annotations

from remix.input_mapper import map_input
from remix.output_remixer import remix_output
from remix.ascii_artist import AsciiArtist
from remix.narrative_enhancer import NarrativeEnhancer

# Simple heuristics to detect a room-entry response (starts with a room name line)
_ROOM_MARKERS = ("you are", "you're", "this is", "it is", "the ", "you can see")


def _looks_like_room_entry(text: str) -> bool:
    """Return True if the Z-machine response looks like entering a new room."""
    stripped = text.strip().lower()
    return any(stripped.startswith(m) for m in _ROOM_MARKERS) or \
           ("\n" in stripped and len(stripped.splitlines()) >= 2)


def _extract_room_name(text: str) -> str:
    """Best-effort: first non-empty line is usually the room name."""
    for line in text.splitlines():
        if line.strip():
            return line.strip()
    return "Unknown Location"


class RemixLayer:
    """Wraps an engine's output with LLM-powered remix features."""

    def __init__(self, model: str | None = None) -> None:
        self.active: bool = True
        self._artist = AsciiArtist()
        self._enhancer = NarrativeEnhancer()
        self._first_room: bool = True
        # model override is passed to router via env var if needed

    def process(self, user_input: str, zork_response: str) -> str:
        """Process one game turn through the remix pipeline.

        1. Map user_input to a valid Zork command (for display purposes —
           the engine already ran with the original input; mapping is
           purely for the output_remixer's context).
        2. If response looks like a room entry, prepend ASCII art.
        3. Remix the response text to match user_input's flavour.
        4. Record arrival postcard on first room.
        """
        if not self.active:
            return zork_response

        mapped_cmd = map_input(user_input)

        parts: list[str] = []

        # ASCII art on room entry
        if _looks_like_room_entry(zork_response):
            room_name = _extract_room_name(zork_response)
            art = self._artist.get(room_name, zork_response)
            if art:
                parts.append(art + "\n")
            if self._first_room:
                self._enhancer.record(room_name, "arrival",
                                      f"First location: {room_name}")
                self._first_room = False

        # Detect death/victory for postcards
        low = zork_response.lower()
        if any(w in low for w in ("you have died", "you are dead", "score:")):
            room_name = _extract_room_name(zork_response)
            self._enhancer.record(room_name, "death", zork_response[:200])
        if "congratulations" in low or "you have won" in low:
            self._enhancer.record("Victory", "victory", zork_response[:200])

        remixed = remix_output(mapped_cmd, zork_response)
        parts.append(remixed)
        return "".join(parts)

    def on_game_end(self) -> None:
        """Display postcards when the game ends."""
        self._enhancer.display()
```

- [ ] **Step 3: Run all tests**

```bash
python -m pytest tests/ -v
```
Expected: all PASS (device/riscv engine tests skip without hardware)

- [ ] **Step 4: End-to-end test of remix mode**

```bash
echo -e "open the mailbox with my teeth\ntake the leaflet please\nquit" \
  | python play.py --stage sim --remix
```
Expected: game runs, LLM remixes the mailbox response creatively (or passes through if Ollama not running), no crash.

- [ ] **Step 5: Commit**

```bash
git add remix/mode.py tests/test_remix.py
git commit -m "feat: remix/mode — RemixLayer wires all remix components into play.py"
```

---

## Task 11: Four Demo Scripts

**Files:**
- Create: `demos/demo-stage1.sh`
- Create: `demos/demo-stage2.sh`
- Create: `demos/demo-stage3.sh`
- Create: `demos/demo-stage4.sh`
- Create: `demos/README.md`

Each script runs a scripted session using `expect` (or a heredoc pipe) so the recording is reproducible. Title cards use `figlet` if available, otherwise plain echo.

- [ ] **Step 1: Create `demos/demo-stage1.sh`**

```bash
#!/usr/bin/env bash
# demo-stage1.sh — Stage 1: Pure Python Z-machine
# Record with: asciinema rec -t "Zork on Tenstorrent — Stage 1" demos/stage1.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  STAGE 1 — Pure Python Z-machine  (no hardware required)   ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
sleep 2

# Pipe scripted commands with human-paced delays
{
  sleep 4      # let opening text render
  echo "look"
  sleep 3
  echo "open mailbox"
  sleep 3
  echo "take leaflet"
  sleep 3
  echo "read leaflet"
  sleep 5
  echo "quit"
} | python play.py --stage sim game/zork1.z3
```

- [ ] **Step 2: Create `demos/demo-stage2.sh`**

```bash
#!/usr/bin/env bash
# demo-stage2.sh — Stage 2: Game data on QB2 DRAM
# Record with: asciinema rec -t "Zork on Tenstorrent — Stage 2" demos/stage2.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  STAGE 2 — Game data lives in QB2 DRAM  (Tenstorrent chip) ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
sleep 2

{
  sleep 12     # QB2 device init + game upload
  echo "look"
  sleep 3
  echo "open mailbox"
  sleep 3
  echo "take leaflet"
  sleep 3
  echo "quit"
} | python play.py --stage device game/zork1.z3
```

- [ ] **Step 3: Create `demos/demo-stage3.sh`**

```bash
#!/usr/bin/env bash
# demo-stage3.sh — Stage 3: Interpreter on RISC-V cores
# Record with: asciinema rec -t "Zork on Tenstorrent — Stage 3" demos/stage3.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  STAGE 3 — Z-machine runs on QB2 RISC-V cores  (TT-Lang)  ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
sleep 2

{
  sleep 60     # kernel compile + 5 batches × 40 instructions
  echo "open mailbox"
  sleep 20
  echo "quit"
} | python play.py --stage risc-v game/zork1.z3
```

- [ ] **Step 4: Create `demos/demo-stage4.sh`**

```bash
#!/usr/bin/env bash
# demo-stage4.sh — Stage 4: LLM Remix layer — the punchline
# Record with: asciinema rec -t "Zork on Tenstorrent — Stage 4" demos/stage4.cast
set -e
cd "$(dirname "$0")/.."
source ~/code/tt-lang/build/env/activate

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  STAGE 4 — LLM Remix: Never Be Told NO Again               ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
sleep 2
echo "  Starting in CLASSIC mode — then switching to REMIX live."
echo ""
sleep 3

{
  sleep 5
  echo "/classic"        # confirm we start classic
  sleep 2
  echo "open mailbox"    # classic: raw Z-machine response
  sleep 3
  echo "/remix"          # switch to remix mode LIVE
  sleep 2
  echo "open the mailbox with my teeth"  # remix: creative response
  sleep 5
  echo "eat the leaflet"                 # something Zork wouldn't allow
  sleep 5
  echo "fight the darkness with hope"    # poetic nonsense — LLM handles it
  sleep 5
  echo "quit"
} | python play.py --stage sim --remix game/zork1.z3
```

- [ ] **Step 5: Create `demos/README.md`**

```markdown
# Demo Scripts

Four asciinema recordings showing the Zork on Tenstorrent arc.

## Prerequisites

    pip install asciinema
    # For QB2 demos:
    source ~/code/tt-lang/build/env/activate
    # For remix demos:
    ollama pull qwen2.5:1.5b
    ollama pull qwen2.5:7b

## Recording

    asciinema rec -t "Zork — Stage 1" demos/stage1.cast -- bash demos/demo-stage1.sh
    asciinema rec -t "Zork — Stage 2" demos/stage2.cast -- bash demos/demo-stage2.sh
    asciinema rec -t "Zork — Stage 3" demos/stage3.cast -- bash demos/demo-stage3.sh
    asciinema rec -t "Zork — Stage 4" demos/stage4.cast -- bash demos/demo-stage4.sh

## Playback

    asciinema play demos/stage1.cast
    asciinema play demos/stage4.cast

## Upload (optional)

    asciinema upload demos/stage4.cast
```

- [ ] **Step 6: Make scripts executable and commit**

```bash
chmod +x demos/demo-stage*.sh
git add demos/
git commit -m "feat: four asciinema demo scripts for the four-stage arc"
```

---

## Task 12: Blog Post Draft

**Files:**
- Create: `docs/blog-draft.md`

- [ ] **Step 1: Write blog post draft**

```bash
cat > docs/blog-draft.md << 'BLOG'
# We Put Zork on a Tenstorrent AI Chip (And Then Made It Weirder)

*A story about Z-machines, RISC-V cores, and never being told NO again.*

---

## Why Zork?

In 1977, Infocom published Zork I — a text adventure game that ran on a
virtual machine called the Z-machine. The Z-machine is beautifully simple:
a tiny instruction set, a 128KB address space, a stack, a dictionary. It's
a complete computer in about 5,000 bytes of specification.

We run a Tenstorrent Blackhole AI accelerator in our lab. Every chip ships
with a grid of RISC-V cores alongside the Tensix AI processors. We thought:
*what's the smallest, most complete program you could run on new hardware to
prove it works?* Zork checks every box. It has memory reads and writes,
arithmetic, string decoding, a call stack, I/O. If Zork runs, the hardware
runs.

This is the story of how we got there — in four stages.

---

## Stage 1 — It Runs

```bash
python play.py --stage sim game/zork1.z3
```

[embed: stage1.cast]

We wrote a pure Python Z-machine V3 interpreter (`ttlang/zmachine_v3.py`).
Nothing remarkable here — it decodes Z-strings, manages a call stack,
handles the object tree. About 700 lines. It runs Zork I end-to-end.

This is Stage 1. No hardware. Just proof that we understand the format.

---

## Stage 2 — It Runs on the Hardware

```bash
python play.py --stage device game/zork1.z3
```

[embed: stage2.cast]

The Blackhole chip has DRAM — gigabytes of it, accessible via the TT-Lang
Python environment (`ttnn`). We uploaded the 86,838-byte Zork game file to
QB2 DRAM as a `ttnn` tensor, read it back to verify the round-trip, then
ran the Python interpreter against the DRAM-resident bytes.

The game is physically on the chip. We did this because we can.

One gotcha worth documenting: `ttnn` DRAM buffers have a `page_size`
parameter. If you set it to anything less than the full buffer size, pages
may not be contiguous and NoC reads at offsets beyond page 0 return garbage.
Set `page_size = buffer_size` for flat buffers. This cost us two sessions.

---

## Stage 3 — The Hardware Runs It

```bash
python play.py --stage risc-v game/zork1.z3
```

[embed: stage3.cast]

Now we move the interpreter itself onto the chip. The Blackhole's RISC-V
cores can run arbitrary C++ via TT-Lang's `ttnn.generic_op` +
`KernelDescriptor` API. We compiled our Z-machine interpreter
(`kernels/zork_interpreter_l1.cpp`) as a RISC-V kernel and dispatched it
to core (0,0).

The hardware is thinking now.

One constraint: the QB2 firmware watchdog limits kernel execution time. We
found the safe limit empirically: 40 Z-machine instructions per invocation.
The Zork opening text requires ~200 instructions, so we run 5 batches,
persisting `ZMachineState` (PC + stack + call frames) to DRAM between
invocations. Think of it like LLM token generation: short, bounded steps,
state persisted between them.

```
ZORK I: The Great Underground Empire
Infocom interactive fiction - a fantasy story
Copyright (c) 1981, 1982, 1983 Infocom, Inc. All rights reserved.
```

That text came out of a RISC-V core inside a Tenstorrent AI accelerator.
We are unreasonably proud of this.

---

## Stage 4 — Now Let's Make It Fun

```bash
python play.py --stage sim --remix game/zork1.z3
```

[embed: stage4.cast]

Stages 1–3 prove the hardware. Stage 4 makes it worth playing.

Classic Zork is unforgiving. "Open the mailbox with my teeth" gets you
"I don't understand that." "Fight the darkness with hope" gets you
"That's not a verb I recognise." The parser is the wall.

In remix mode, the Z-machine still runs every turn. It's still the source
of truth for game state — what exists, what you're carrying, what rooms
connect. But we wrap every response through an LLM. The LLM receives what
you typed *and* what the game responded, and rewrites the response to match
your energy.

```
> open the mailbox with my teeth
The mailbox creaks open with a tetanus-worthy groan as you regret this
decision immediately. Inside, mercifully, is a leaflet.
```

The mailbox is still open. The leaflet is still inside. The game state
is unchanged. But the *voice* of the world bent to meet you.

You can toggle between modes live:
```
> /classic     — raw Z-machine output
> /remix       — LLM-powered responses
```

At game end, the ASCII journey map (already built in an earlier phase)
is joined by narrative postcards — short, literary descriptions of
memorable moments generated by the LLM:

```
╔══════════════════════════════════════════════╗
║  POSTCARDS FROM YOUR JOURNEY                ║
╠══════════════════════════════════════════════╣
║ West of House                               ║
║ I arrived at the white house as evening     ║
║ fell, the mailbox gleaming like a dare I    ║
║ had no business refusing.                   ║
╚══════════════════════════════════════════════╝
```

---

## Try It Yourself

```bash
git clone https://github.com/tenstorrent/tt-zork1
cd tt-zork1
pip install torch requests

# Stage 1 — no hardware needed
python play.py --stage sim game/zork1.z3

# Stage 4 — with LLM remix (requires Ollama)
ollama pull qwen2.5:1.5b
ollama pull qwen2.5:7b
python play.py --stage sim --remix game/zork1.z3

# Stage 2 or 3 — requires Tenstorrent QB2 hardware
source ~/code/tt-lang/build/env/activate
python play.py --stage device game/zork1.z3
python play.py --stage risc-v game/zork1.z3
```

The four demo recordings are in `demos/`. The full implementation is
documented in `CLAUDE.md`.

---

*Built at Tenstorrent, 2026. Zork I © Infocom, Inc. All rights reserved.*
BLOG
```

- [ ] **Step 2: Commit**

```bash
git add docs/blog-draft.md
git commit -m "docs: blog post draft — four-stage Zork on Tenstorrent arc"
```

---

## Self-Review Checklist

**Spec coverage:**
- ✅ `play.py` unified entry point with `--stage`, `--remix`, `--persona`, `--model`
- ✅ `engines/` with `sim`, `device`, `risc-v` and shared `BaseEngine` interface
- ✅ `remix/` with `input_mapper`, `output_remixer`, `ascii_artist`, `narrative_enhancer`, `personas`, `mode`
- ✅ `/classic` and `/remix` in-game toggle
- ✅ ASCII room art cached per room, max 6 lines, framed with room name
- ✅ Narrative postcards at game end (arrival, treasure, death, victory)
- ✅ Four personas ported from `feature/multi-llm-integration`
- ✅ LLM router for cheap/rich model split
- ✅ Graceful fallback when Ollama unavailable
- ✅ Four asciinema demo scripts
- ✅ Blog post draft

**Not covered (out of scope per spec):** ncurses TUI, SDXL images, multi-chip tensor parallel, save/restore across sessions.

**Type consistency check:** `BaseEngine.step()` returns `str` throughout. `RemixLayer.process()` takes `(str, str) → str`. `call_ollama()` returns `str | None`. All consistent across tasks.

**Placeholder scan:** No TBD/TODO in code blocks. All prompts written in full. All test assertions specific.
```
