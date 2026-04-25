# TUI Status Layer — Implementation Design

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a `--tui` flag to `play.py` that launches a Textual-based full-terminal UI with a side panel showing live LLM token streaming, room ASCII art, and hardware telemetry — while leaving the plain terminal mode fully intact for pipe-based demo scripts.

**Architecture:** A `ZMachineTuiApp(App)` wraps the existing engine and remix layer unchanged. The game loop runs in a background thread, posting output to Textual's event system. A streaming variant of `call_llm` feeds the THINKING panel token-by-token. A `set_interval` worker polls `tt-smi -s` for hardware state. The right panel is a state machine: THINKING > ART > HARDWARE.

**Tech Stack:** Textual ≥ 0.61 (already in `requirements.txt` via tt-model-runner patterns), `requests` with `stream=True` for SSE token streaming, `ttlang.zmachine_v3.ZMachineV3` for vocabulary extraction.

**Design principle:** No game-specific knowledge in the TUI layer. The game file's own Z-machine dictionary table drives token colorization — works for any V3 title, not just Zork.

---

## 1. File Structure

**New files:**
```
tui/__init__.py
tui/app.py            — ZMachineTuiApp(App): composes panes, owns game thread
tui/game_pane.py      — left scrolling RichLog: receives game text events
tui/context_pane.py   — right adaptive panel: THINKING / ART / HARDWARE states
tui/hw_poller.py      — parses tt-smi -s JSON → HardwareSnapshot dataclass
tui/vocabulary.py     — extracts Z-machine dictionary from game binary at startup
```

**Modified files:**
```
play.py               — add --tui flag; route to ZMachineTuiApp when set
remix/llm.py          — add call_llm_stream() generator alongside call_llm()
remix/output_remixer.py — add remix_output_stream() that yields chunks
```

No changes to engines/, remix/mode.py, or any other existing file.

---

## 2. `play.py` changes

Add one argument:
```python
parser.add_argument("--tui", action="store_true",
                    help="Launch Textual TUI (side panel with art, LLM stream, hardware)")
```

In `main()`, after building engine and remix_layer:
```python
if args.tui:
    from tui.app import ZMachineTuiApp
    ZMachineTuiApp(engine=engine, remix_layer=remix_layer,
                   game_path=args.game).run()
else:
    game_loop(engine, remix_layer)   # unchanged
```

`ZMachineTuiApp.run()` must call `engine.close()` on exit (matching the `finally` in `main()`).

---

## 3. `remix/llm.py` — streaming addition

Add `call_llm_stream()` alongside the existing blocking `call_llm()`. The existing function is **not modified**.

```python
def call_llm_stream(
    system: str,
    user: str,
    model: str = "meta-llama/Llama-3.1-8B-Instruct",
    temperature: float = 0.7,
) -> Iterator[str]:
    """Yield text chunks as they stream from the server (SSE).

    Yields empty iterator on any error — callers must handle gracefully.
    Uses requests with stream=True and parses OpenAI-compatible SSE lines:
        data: {"choices":[{"delta":{"content":"token"}}]}
    Stops on `data: [DONE]`.
    """
```

Error handling: any connection error, timeout, or malformed chunk silently stops the iterator. Callers fall back to the blocking `call_llm()` result if the stream yields nothing.

---

## 4. `remix/output_remixer.py` — streaming addition

Add `remix_output_stream()` alongside the existing `remix_output()`:

```python
def remix_output_stream(
    user_input: str,
    game_response: str,
    on_token: Callable[[str], None],
) -> str:
    """Stream remix tokens to on_token callback; return full remixed text.

    Falls back to blocking call_llm() if stream yields nothing, returning
    the full response without calling on_token at all. Always returns the
    complete final string so the caller has the result regardless of path.
    """
```

`remix/mode.py` (`RemixLayer.process()`) is **not modified**. In TUI mode, `ZMachineTuiApp._run_game()` replicates the `RemixLayer.process()` orchestration in a streaming-aware way:

1. Call `map_input(user_input)` → `mapped_cmd`
2. Check `_looks_like_room_entry(game_response)` — if true, post `StreamStart(task="art")`, stream `AsciiArtist.get()` via `call_llm_stream()`, post `StreamDone(task="art", room_name=...)`
3. Post `StreamStart(task="remix")`, call `remix_output_stream(mapped_cmd, game_response, on_token=...)`, post `StreamDone(task="remix")`
4. Detect death/victory and call `NarrativeEnhancer.record()` as before

`RemixLayer.process()` continues to serve the plain terminal path unchanged.

---

## 5. `tui/vocabulary.py`

Extracts the Z-machine V3 dictionary table from a loaded `ZMachineV3` instance at startup. Returns a `frozenset[str]` of all dictionary words.

```python
def extract_vocabulary(zm: ZMachineV3) -> frozenset[str]:
    """Read the Z-machine dictionary table from the loaded game binary.

    Z-machine V3 dictionary format (spec §13):
      header offset 0x08 → word address of dictionary
      dictionary layout: separator-count, separators, entry-length,
                         entry-count, then entry-count × entry-length entries.
      Each entry starts with a 4-byte encoded Z-string (2 words).
    Returns a frozenset of lowercase plain-text words.
    """
```

Called once in `ZMachineTuiApp.__init__()`. The result is passed to `context_pane.py` for use by the colorizer. If extraction fails (malformed game or unsupported format), returns `frozenset()` — colorizer degrades gracefully to heuristics-only.

---

## 6. `tui/hw_poller.py`

```python
@dataclass
class HardwareSnapshot:
    tensix_pct: float        # 0–100, -1 if unavailable
    riscv_pct: float         # 0–100, -1 if unavailable
    dram_read_gbps: float    # -1 if unavailable
    dram_write_gbps: float   # -1 if unavailable
    temp_c: float            # -1 if unavailable
    power_w: float           # -1 if unavailable
    stage_label: str         # e.g. "Stage 3 — RISC-V cores"

def poll() -> HardwareSnapshot:
    """Run tt-smi -s, parse JSON, return HardwareSnapshot.

    Returns a zeroed snapshot with all fields -1 if tt-smi is unavailable
    or returns non-zero exit code. Never raises.
    """
```

Called by a Textual `set_interval(1.0)` worker in `ZMachineTuiApp`. The worker posts a custom `HardwareUpdate` message to `context_pane.py`.

The `stage_label` field is set by `ZMachineTuiApp` (it knows which engine is active) before the snapshot is posted — the poller itself is stage-agnostic.

---

## 7. `tui/context_pane.py` — right panel

### State machine

```
State: HARDWARE  (default)
  → THINKING     when TUI receives StreamStart message
      → ART       when TUI receives StreamDone(task="art")
      → HARDWARE  when TUI receives StreamDone(task=other)
  → ART           when TUI receives RoomEntry message (art cached per room)
      → THINKING  when next StreamStart arrives
```

### THINKING state — token colorizer

Tokens are classified as they arrive:

1. **Teal** (`#4fd1c5`): token (lowercased, stripped of punctuation) is in the game vocabulary `frozenset`
2. **Pink** (`#ec96b8`): token passes vivid heuristic — length ≥ 8 AND ends in `-ous`, `-ful`, `-ing`, `-ish`, `-ent`, `-ive`, or `-ly`
3. **Amber** (`#f4c471`): token is a standalone capitalised word mid-sentence (not sentence-start), or immediately follows "a", "an", "the", "this", "that"
4. **Dim grey** (`#607d8b`): articles, prepositions, conjunctions — matched against a ~20-word stopword list
5. **Default** (`#e8f0f2`): everything else

Classification is applied per whitespace-delimited token. Punctuation is split off before lookup, re-attached before display. Processing is O(n) per token — no NLP library required.

### ART state

Displays the ASCII art string stored in `RemixLayer._artist._cache` for the current room. The `ZMachineTuiApp` reads the cache after `StreamDone(task="art", room_name=...)` and posts a `ShowArt(text, room_name)` message. If the cache entry is empty (art failed), transitions to HARDWARE instead.

Art is displayed in a `ScrollableContainer` within `ContextPane` — lines wider than 40 columns are soft-wrapped (not truncated) so no content is lost on narrow terminals.

### HARDWARE state

Renders `HardwareSnapshot` as a series of labelled progress bars using Textual's `ProgressBar` widget. Bar colours:
- Tensix: green (`#27ae60`) when > 10%, grey otherwise
- RISC-V: amber (`#f4c471`) when > 10%, grey otherwise
- DRAM: teal (`#4fd1c5`) read, muted teal write
- Temp/power: plain text

When all fields are -1 (no hardware / Stage 1), renders a single grey label: `pure Python — no hardware`.

---

## 8. `tui/game_pane.py`

A `RichLog` widget that receives `GameText(content: str)` messages posted from the game thread. Strips trailing whitespace. Appends a blank line between turns for readability.

The `>` prompt is rendered in dim grey (not repeated — Textual's `Input` widget at the bottom handles player input). Player input echoes in `#607d8b`, game output in `#e8f0f2`.

---

## 9. `tui/app.py` — ZMachineTuiApp

### Layout (Textual CSS)

```
Screen {
    layout: horizontal;
}
GamePane {
    width: 1fr;          /* ~60% on typical terminal */
    border-right: solid $primary-darken-2;
}
ContextPane {
    width: 40;           /* fixed 40 cols — enough for art + bars */
}
```

Bottom: Textual `Footer` showing key bindings. A single-line `Static` status bar above the footer:
```
Stage 2 · Classic · QB2 DRAM · :8001 OK · 4 commands
```
Updated after each turn.

### Game thread

```python
def _run_game(self) -> None:
    """Run in executor thread. Posts messages to Textual event loop."""
    out = self._engine.startup()
    self.call_from_thread(self.post_message, GameText(out))

    while True:
        # blocks on Input widget result via asyncio.Queue
        cmd = self._input_queue.get()
        if cmd is None:  # sentinel for quit
            break
        # ... step engine, post GameText, handle StreamStart/Done
```

Input comes from a Textual `Input` widget at the bottom of `GamePane`. On `Input.Submitted`, the command is put into `_input_queue` and the input widget is cleared + disabled until the response arrives.

### Key bindings

```
/   → focus Input widget (shortcut to start typing)
F1  → toggle context pane visibility
q   → quit (with confirmation if game in progress)
```

`/classic` and `/remix` are handled as game commands (passed through to `_handle_meta` unchanged).

---

## 10. Demo script integration

The four `demos/demo-stage*.sh` scripts are **not modified** — they pipe stdin and work without `--tui`.

`record-all.sh` gets a note that adding `--tui` to the demo scripts produces richer recordings. The user can add `--tui` when re-recording for the blog.

---

## 11. Error handling and graceful degradation

| Failure | Behaviour |
|---------|-----------|
| `textual` not installed | `play.py --tui` prints "pip install textual" and exits cleanly |
| `tt-smi` not found | Hardware panel shows "tt-smi unavailable" in grey, no crash |
| LLM server down | THINKING panel shows "LLM unavailable", game continues in classic mode |
| Stream yields nothing | Falls back to blocking `call_llm()`, result posted as single `GameText` |
| Art generation fails | Context pane transitions to HARDWARE instead of ART |
| Vocabulary extraction fails | Colorizer uses heuristics-only, no game objects highlighted teal |

---

## 12. Testing

- `tests/test_tui_vocabulary.py` — vocabulary extraction returns non-empty frozenset for `game/zork1.z3`; known words (`mailbox`, `north`) are present
- `tests/test_tui_colorizer.py` — known words → teal; stopwords → grey; vivid suffix → pink; unknown noun → amber; empty vocabulary → heuristics only
- `tests/test_llm_stream.py` — `call_llm_stream()` mocked SSE response yields correct chunks; empty stream falls back; connection error yields empty
- `tests/test_hw_poller.py` — mock `tt-smi -s` JSON → correct `HardwareSnapshot`; malformed JSON → zeroed snapshot, no raise
