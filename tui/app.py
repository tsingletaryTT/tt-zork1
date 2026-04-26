# tui/app.py
"""ZMachineTuiApp — Textual application wrapping the Z-machine game loop.

Layout (horizontal):
    GamePane (1fr)  │  ContextPane (40 cols fixed)

Below: Static status bar + Footer.

The game runs in a background executor thread (_run_game). All state changes
are posted to the Textual event loop via call_from_thread + post_message.

Custom messages (module-level, not nested):
    GameText      — game/remix output to display in GamePane
    TokenReceived — one streaming LLM token to the ContextPane THINKING display
    StreamStart   — LLM call started (task: "art"|"remix")
    StreamDone    — LLM call finished (task, room_name)
    HardwareUpdate— fresh HardwareSnapshot from tt-smi poll
    ShowArt       — ASCII art ready after StreamDone("art")
"""

import asyncio
import queue
import threading
from pathlib import Path

from textual.app import App, ComposeResult
from textual.binding import Binding
from textual.message import Message
from textual.widgets import Footer, Input, Static
from textual.containers import Horizontal

from tui.game_pane import GamePane
from tui.context_pane import ContextPane
from tui.hw_poller import poll as hw_poll, HardwareSnapshot
from tui.vocabulary import extract_vocabulary

# Lazy art system prompt — read once at import time, shared across all art LLM calls.
_ART_SYSTEM_PATH = Path(__file__).parent.parent / "prompts" / "ascii_system.txt"


# ---------------------------------------------------------------------------
# Custom messages — defined at module level (not nested) so Textual's message
# routing resolves the handler names correctly via on_<class_name>().
# ---------------------------------------------------------------------------

class GameText(Message):
    """Carry a chunk of Z-machine or remixed text to write into GamePane."""

    def __init__(self, content: str) -> None:
        super().__init__()
        self.content = content


class TokenReceived(Message):
    """Carry a single streamed LLM token to the ContextPane THINKING display."""

    def __init__(self, text: str) -> None:
        super().__init__()
        self.text = text


class StreamStart(Message):
    """Signal that an LLM stream has started.

    task: arbitrary label shown in the ContextPane header (e.g. "art", "remix").
    """

    def __init__(self, task: str) -> None:
        super().__init__()
        self.task = task


class StreamDone(Message):
    """Signal that an LLM stream has completed.

    task:      the same label passed to StreamStart.
    room_name: the room that triggered an "art" stream; empty for other tasks.
    """

    def __init__(self, task: str, room_name: str = "") -> None:
        super().__init__()
        self.task = task
        self.room_name = room_name


class HardwareUpdate(Message):
    """Carry a fresh HardwareSnapshot from the tt-smi poller to ContextPane."""

    def __init__(self, snapshot: HardwareSnapshot) -> None:
        super().__init__()
        self.snapshot = snapshot


class ShowArt(Message):
    """Carry framed ASCII art to ContextPane after generation is complete."""

    def __init__(self, text: str, room_name: str) -> None:
        super().__init__()
        self.text = text
        self.room_name = room_name


# ---------------------------------------------------------------------------
# Stage label mapping (engine._stage → human-readable string for status bar)
# ---------------------------------------------------------------------------

_STAGE_LABELS = {
    "sim":    "Stage 1 — Pure Python",
    "device": "Stage 2 — QB2 DRAM",
    "risc-v": "Stage 3 — RISC-V cores",
}


# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------


class ZMachineTuiApp(App[None]):
    """Full-terminal TUI for any Z-machine V3 game.

    The app is responsible for:
    - Composing the two-pane layout (GamePane left, ContextPane right).
    - Starting and coordinating the background game thread (_run_game).
    - Polling hardware telemetry every second via set_interval.
    - Routing messages between the game thread and the UI widgets.
    - Handling player input (Input.Submitted) and meta-commands (prefixed "/").

    Args:
        engine:       A loaded BaseEngine instance (sim, device, or RISC-V).
        remix_layer:  A RemixLayer, or None to run in classic mode only.
        game_path:    Path to the .z3 file; used for vocabulary extraction.
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

    def __init__(
        self,
        engine,
        remix_layer=None,
        game_path: str = "game/zork1.z3",
    ) -> None:
        super().__init__()
        self._engine = engine
        self._remix_layer = remix_layer
        self._game_path = game_path

        # Thread-safe queue: game thread blocks here waiting for player input.
        self._input_queue: queue.Queue = queue.Queue()
        self._turn_count = 0

        # Which engine stage are we running? (Used in the status bar.)
        self._stage = getattr(engine, "_stage", "sim")

        # Auto-play persona name when active; None means manual play.
        # Written by the game thread, read by _update_status on the event loop.
        self._auto_persona_name: str | None = None

        # Build the Z-machine vocabulary for ContextPane token colorizer.
        try:
            from ttlang.zmachine_v3 import ZMachineV3
            zm = ZMachineV3(Path(game_path).read_bytes())
            self._vocabulary = extract_vocabulary(zm)
        except Exception:
            # Vocabulary is best-effort; the colorizer degrades to no teal.
            self._vocabulary = frozenset()

        # Read the ASCII art system prompt once.  If the file is missing the
        # art generation path is simply skipped (empty system prompt → no art).
        try:
            self._art_system = _ART_SYSTEM_PATH.read_text()
        except Exception:
            self._art_system = ""

    # ------------------------------------------------------------------
    # Textual compose / mount
    # ------------------------------------------------------------------

    def compose(self) -> ComposeResult:
        """Build the widget tree: two panes, a status bar, and footer."""
        with Horizontal(id="main-row"):
            yield GamePane(id="game")
            # vocabulary= must be passed as a keyword argument (not positional)
            # because ContextPane.__init__ uses **kwargs for the Widget params.
            yield ContextPane(id="context", vocabulary=self._vocabulary)
        yield Static("", id="status-bar")
        yield Footer()

    def on_mount(self) -> None:
        """Kick off hardware polling and the background game thread."""
        self._update_status()
        # Hardware poll fires every second without blocking the event loop.
        self.set_interval(1.0, self._tick_hardware)
        # Game thread runs as a daemon so it dies automatically when the app exits.
        self._game_thread = threading.Thread(target=self._run_game, daemon=True)
        self._game_thread.start()

    # ------------------------------------------------------------------
    # Hardware polling (called on the Textual event loop by set_interval)
    # ------------------------------------------------------------------

    async def _tick_hardware(self) -> None:
        """Run tt-smi in a thread-pool executor and post a HardwareUpdate."""
        loop = asyncio.get_running_loop()
        # hw_poll() is a blocking subprocess call; run_in_executor prevents it
        # from stalling the Textual event loop.
        snap = await loop.run_in_executor(None, hw_poll)
        # stage_label is filled here (not inside hw_poller) because only the
        # app knows which engine is active.
        snap.stage_label = _STAGE_LABELS.get(self._stage, self._stage)
        self.post_message(HardwareUpdate(snap))

    # ------------------------------------------------------------------
    # Message handlers — route each message to the appropriate widget
    # ------------------------------------------------------------------

    def on_game_text(self, msg: GameText) -> None:
        """Append game or remix output to the scrolling GamePane log."""
        self.query_one(GamePane).write_game_text(msg.content)

    def on_token_received(self, msg: TokenReceived) -> None:
        """Forward a streaming LLM token to ContextPane's THINKING display."""
        self.query_one(ContextPane).on_token(msg.text)

    def on_stream_start(self, msg: StreamStart) -> None:
        """Transition ContextPane to THINKING state for the given task."""
        self.query_one(ContextPane).on_stream_start(msg.task)

    def on_stream_done(self, msg: StreamDone) -> None:
        """Signal ContextPane that the LLM stream is finished."""
        self.query_one(ContextPane).on_stream_done(msg.task)

    def on_hardware_update(self, msg: HardwareUpdate) -> None:
        """Push fresh telemetry into ContextPane's HARDWARE display."""
        self.query_one(ContextPane).on_hardware_update(msg.snapshot)

    def on_show_art(self, msg: ShowArt) -> None:
        """Display cached ASCII art in ContextPane."""
        self.query_one(ContextPane).on_show_art(msg.text, msg.room_name)

    # ------------------------------------------------------------------
    # Input handling (bubbles up from GamePane's Input widget)
    # ------------------------------------------------------------------

    def on_input_submitted(self, event: Input.Submitted) -> None:
        """Route player input: meta-commands are handled here; game commands
        are placed on the queue for the game thread to consume."""
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

        # Disable the input field while the game thread processes the command;
        # _enable_input() re-enables it once the response arrives.
        event.input.disabled = True
        self._input_queue.put(cmd)

    def _handle_meta(self, cmd: str):
        """Process a /slash meta-command.

        Returns:
            (action, message_text) where action is "" or "quit" and
            message_text is a short status string to display in the game pane.
        """
        c = cmd.lower().strip()
        if c == "/quit":
            return "quit", "[Goodbye!]"
        if c == "/help":
            return "", (
                "Commands: /classic  /remix  /auto [persona]  /stop  /help  /quit\n"
                "  /auto [expert|naive|completionist|experimental] — start LLM auto-play\n"
                "  /stop — stop auto-play and return to manual mode"
            )
        if c == "/remix" and self._remix_layer:
            self._remix_layer.active = True
            return "", "[Remix mode ON]"
        if c == "/classic" and self._remix_layer:
            self._remix_layer.active = False
            return "", "[Classic mode]"
        if c.startswith("/auto"):
            parts = cmd.split(None, 1)
            persona_name = parts[1].lower() if len(parts) > 1 else "expert"
            self._input_queue.put(f"AUTO:{persona_name}")
            return "", f"[Auto-play starting: {persona_name}]"
        if c == "/stop":
            self._input_queue.put("STOP_AUTO")
            return "", "[Stopping auto-play after current move]"
        return "", f"[Unknown: {cmd}]"

    # ------------------------------------------------------------------
    # Game thread — runs in the background, posts Textual messages back
    # ------------------------------------------------------------------

    def _run_game(self) -> None:
        """Drive the Z-machine engine and post UI messages to the event loop.

        This method normally blocks on self._input_queue.get() between turns.
        When auto-play is active (_auto_name is set), the LLM picks the next
        command and re-queues it without waiting for player input.

        Special queue sentinels:
            None          — shutdown (posted by action_quit_confirm)
            "STOP_AUTO"   — stop auto-play, return to manual mode
            "AUTO:<name>" — start auto-play with the named persona

        All Textual mutations must go through call_from_thread() so they are
        safe to call from this non-Textual thread.
        """
        from remix.mode import _looks_like_room_entry, _extract_room_name
        from remix.llm import call_llm, call_llm_stream
        from remix.router import route
        from remix.ascii_artist import _frame as _frame_art

        # Local flag: persona name when auto-play is active, else None.
        # Also mirrored to self._auto_persona_name for the status bar.
        _auto_name: str | None = None

        # --- startup ---
        out = self._engine.startup()
        self.call_from_thread(self.post_message, GameText(out))
        self.call_from_thread(self._enable_input)

        while True:
            cmd = self._input_queue.get()

            # --- Sentinel: shutdown ---
            if cmd is None:
                break

            # --- Sentinel: stop auto-play ---
            if cmd == "STOP_AUTO":
                _auto_name = None
                self._auto_persona_name = None
                self.call_from_thread(self._enable_input)
                self.call_from_thread(self._update_status)
                continue

            # --- Sentinel: start auto-play ---
            if cmd.startswith("AUTO:"):
                _auto_name = cmd[5:].lower()
                self._auto_persona_name = _auto_name
                self.call_from_thread(self._update_status)
                # Kick off with "look" so the LLM sees the current room.
                cmd = "look"

            game_response = self._engine.step(cmd)

            if not self._remix_layer or not self._remix_layer.active:
                # --- Classic mode: show raw Z-machine output unchanged ---
                self.call_from_thread(self.post_message, GameText(game_response))

            else:
                # --- Remix mode ---
                from remix.input_mapper import map_input
                mapped_cmd = map_input(cmd)

                if _looks_like_room_entry(game_response):
                    room_name = _extract_room_name(game_response)
                    key = room_name.strip().lower()

                    # AsciiArtist._cache is a plain dict; check directly to
                    # avoid a duplicate LLM call when art is already cached.
                    artist_cache = self._remix_layer._artist._cache

                    if key not in artist_cache and self._art_system:
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
                        framed = _frame_art(raw_art, room_name) if raw_art else ""
                        artist_cache[key] = framed

                        self.call_from_thread(
                            self.post_message, StreamDone("art", room_name=room_name)
                        )
                        if framed:
                            self.call_from_thread(
                                self.post_message, ShowArt(framed, room_name)
                            )

                    elif key in artist_cache and artist_cache[key]:
                        cached = artist_cache[key]
                        self.call_from_thread(
                            self.post_message, ShowArt(cached, room_name)
                        )

                self.call_from_thread(self.post_message, StreamStart("remix"))
                from remix.output_remixer import remix_output_stream

                def _on_tok(t: str) -> None:
                    self.call_from_thread(self.post_message, TokenReceived(t))

                final_text = remix_output_stream(
                    mapped_cmd, game_response, on_token=_on_tok
                )
                self.call_from_thread(self.post_message, StreamDone("remix"))
                self.call_from_thread(self.post_message, GameText(final_text))
                # Use final_text as context for the persona's next command.
                game_response = final_text

            self._turn_count += 1

            if self._engine.game_over:
                self._auto_persona_name = None
                # Show postcards in the game pane if the remix layer collected any.
                if self._remix_layer:
                    rendered = self._remix_layer.render_postcards()
                    if rendered:
                        self.call_from_thread(
                            self.post_message, GameText("\n" + rendered + "\n")
                        )
                self.call_from_thread(self._update_status)
                break

            # --- Auto-play: LLM picks next command ---
            if _auto_name:
                from remix.personas import get_persona
                try:
                    persona = get_persona(_auto_name)
                except ValueError:
                    _auto_name = None
                    self._auto_persona_name = None
                    self.call_from_thread(self._enable_input)
                    self.call_from_thread(self._update_status)
                    continue

                self.call_from_thread(self.post_message, StreamStart("persona"))
                next_cmd = call_llm(
                    system=persona["system_prompt"],
                    user=game_response,
                    model=route("persona"),
                    temperature=0.7,
                )
                self.call_from_thread(self.post_message, StreamDone("persona"))

                if next_cmd and next_cmd.strip():
                    display_cmd = next_cmd.strip()
                    self.call_from_thread(
                        self.post_message,
                        GameText(f"\n[{persona['name']}] > {display_cmd}\n"),
                    )
                    self._input_queue.put(display_cmd)
                else:
                    # LLM unavailable — fall back to manual play.
                    _auto_name = None
                    self._auto_persona_name = None
                    self.call_from_thread(
                        self.post_message,
                        GameText("[Auto-play: LLM unavailable — returning to manual mode]\n"),
                    )
                    self.call_from_thread(self._enable_input)
            else:
                self.call_from_thread(self._enable_input)

            self.call_from_thread(self._update_status)

    # ------------------------------------------------------------------
    # Helpers called from the event loop (safe to touch widgets directly)
    # ------------------------------------------------------------------

    def _enable_input(self) -> None:
        """Re-enable and focus the player input field after a turn completes."""
        try:
            inp = self.query_one("#game-input", Input)
            inp.disabled = False
            inp.focus()
        except Exception:
            pass

    def _update_status(self) -> None:
        """Refresh the bottom status bar with current mode / turn info."""
        from remix.llm import TT_INFERENCE_URL
        stage = _STAGE_LABELS.get(self._stage, self._stage)
        mode = "Remix" if (self._remix_layer and self._remix_layer.active) else "Classic"
        host = TT_INFERENCE_URL.split("//")[-1].split("/")[0]
        auto = f" · Auto:{self._auto_persona_name}" if self._auto_persona_name else ""
        bar = f"{stage} · {mode}{auto} · {host} · {self._turn_count} turns"
        try:
            self.query_one("#status-bar", Static).update(bar)
        except Exception:
            pass

    # ------------------------------------------------------------------
    # Actions (bound via BINDINGS)
    # ------------------------------------------------------------------

    def action_focus_input(self) -> None:
        """Move keyboard focus to the player command input field."""
        try:
            self.query_one("#game-input", Input).focus()
        except Exception:
            pass

    def action_toggle_context(self) -> None:
        """Show or hide the right-hand ContextPane (F1 toggle)."""
        ctx = self.query_one(ContextPane)
        ctx.display = not ctx.display

    def action_quit_confirm(self) -> None:
        """Post a sentinel to stop the game thread, then exit the app."""
        # The None sentinel tells _run_game to break out of its loop cleanly.
        self._input_queue.put(None)
        self.exit()
