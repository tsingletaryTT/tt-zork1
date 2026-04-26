# tui/context_pane.py
"""ContextPane — right panel with persistent ASCII art, activity bar,
state header, scrollable body, and a pinned hardware footer.

Layout:
    ┌──────────────────────────┐
    │  #art-panel (hidden      │  ← persists across state changes;
    │   until first art)       │    only replaced on next room entry
    ├──────────────────────────┤
    │  #activity-bar (1 line)  │  ← LLM task + word count (dim)
    │  #ctx-header  (1 line)   │  ← THINKING / DONE / LOG label (teal)
    │  #ctx-body    (1fr)      │  ← scrollable info content
    ├──────────────────────────┤
    │  #hw-footer   (2 lines)  │  ← always shows temp/power (dim, pinned)
    └──────────────────────────┘

Info panel state machine:
    IDLE (default)
      → THINKING  on StreamStart
          → (linger 8s, header = DONE)
          → IDLE    after end_linger() timer
      → LOG       on toggle_log() / F2
          → IDLE   on toggle_log() again

Art panel:
    Hidden until ShowArt message; updated in place (no info panel state
    change).  Cleared (hidden) when StreamStart("art") fires for a new room.

Hardware footer:
    Always updated on every HardwareUpdate — never gated by state.  Shows
    compact "temp  N°C    power  NW" text (no progress bars).

Token colorizer rules (classify_token) — in priority order:
    1. teal  #4fd1c5  — word in Z-machine vocabulary frozenset
    2. grey  #607d8b  — articles, prepositions, conjunctions (stopwords)
    3. pink  #ec96b8  — length ≥ 8 AND ends in vivid suffix
    4. amber #f4c471  — capitalised mid-sentence OR follows a/an/the/this/that
    5. default #e8f0f2 — everything else

Grey before pink so common short words like "going" stay dim even though
they end in a vivid suffix.
"""
from textual.app import ComposeResult
from textual.widget import Widget
from textual.widgets import Static

# ---------------------------------------------------------------------------
# Token colorizer
# ---------------------------------------------------------------------------

# Common low-information words that should appear visually subdued so the
# player's eye is drawn to meaningful words instead.
_STOPWORDS = frozenset({
    "a", "an", "the", "this", "that", "these", "those",
    "is", "are", "was", "were", "be", "been",
    "in", "on", "at", "to", "of", "and", "or", "but",
    "it", "you", "i", "we", "he", "she", "they",
    "with", "for", "from", "by", "not",
})

# Adjective/adverb suffixes associated with vivid, evocative prose.  Only
# applied to long words (≥ 8 characters) to avoid false positives on
# common short words like "going" or "truly".
_VIVID_ENDS = ("ous", "ful", "ing", "ish", "ent", "ive", "ly")

# Words that signal a noun phrase is starting.  If the previous token is one
# of these, the current token is colored amber as a potential key noun.
_CONTEXT_NOUNS = frozenset({"a", "an", "the", "this", "that"})


def classify_token(
    token: str,
    vocabulary: frozenset[str],
    prev_token: str = "",
    is_sentence_start: bool = False,
) -> str:
    """Return a hex color code for this display token.

    Priority order:
        1. Game vocabulary  → teal  #4fd1c5
        2. Stopword         → grey  #607d8b
        3. Vivid suffix     → pink  #ec96b8
        4. Noun-phrase cue  → amber #f4c471
        5. Default          → #e8f0f2

    Args:
        token:             One whitespace-delimited word (may include leading/
                           trailing punctuation such as commas and quotes).
        vocabulary:        Game dictionary frozenset (typically from
                           tui/vocabulary.py).  Pass frozenset() to disable
                           teal highlighting.
        prev_token:        The bare (already stripped + lowercased) word that
                           appeared immediately before this one.  Used for
                           the "follows article" amber rule.
        is_sentence_start: True when this token begins a new sentence.  Used
                           to suppress the "capitalised mid-sentence" amber
                           rule at sentence boundaries.

    Returns:
        A CSS hex color string, e.g. "#4fd1c5".
    """
    # Strip common punctuation before vocabulary / suffix checks so that
    # "mailbox," still matches the vocab entry "mailbox".
    clean = token.strip(".,!?;:'\"-()[]").lower()
    if not clean:
        return "#e8f0f2"

    # Rule 1: game vocabulary (teal) — checked first so vocab words always
    # stand out even when they coincidentally have vivid suffixes.
    if clean in vocabulary:
        return "#4fd1c5"

    # Rule 4 (stopwords, checked before vivid suffix): keeps very common words
    # like "going" visually dim even though they match the "ing" suffix.
    if clean in _STOPWORDS:
        return "#607d8b"

    # Rule 2: vivid descriptive suffix + minimum length guard (pink).
    if len(clean) >= 8 and any(clean.endswith(s) for s in _VIVID_ENDS):
        return "#ec96b8"

    # Rule 3: capitalised mid-sentence OR following a determiner/article
    # (amber).  The is_sentence_start guard prevents marking the first word
    # of every sentence amber just because it is capitalised.
    if (token[0].isupper() and not is_sentence_start) or prev_token.lower() in _CONTEXT_NOUNS:
        return "#f4c471"

    # Rule 5: default off-white.
    return "#e8f0f2"


# ---------------------------------------------------------------------------
# Internal state constants
# ---------------------------------------------------------------------------

# IDLE replaces what was previously called HARDWARE: when no stream is active
# and the log is closed, the header and body are empty.  Hardware is always
# shown in the pinned #hw-footer regardless of this state.
_STATE_IDLE = "idle"
_STATE_THINKING = "thinking"
_STATE_LOG = "log"

_COLOR_HEADER_THINKING = "#4fd1c5"


# ---------------------------------------------------------------------------
# ContextPane widget
# ---------------------------------------------------------------------------


class ContextPane(Widget):
    """Right panel that cycles between THINKING, ART, and IDLE states.

    THINKING — streams LLM tokens with per-word color highlighting via
               classify_token().  Activity bar shows task name + word count.
    ART      — displays cached ASCII art for the current room (generated
               off-screen by the LLM with task="art").
    IDLE     — header and body are empty.  Hardware is always visible in the
               pinned #hw-footer at the bottom.

    Transitions are driven by the app calling the public on_*() methods:

        on_stream_start(task)         → enter THINKING
        on_token(text)                → append colored tokens (THINKING only)
        on_stream_done(task)          → exit THINKING → linger → IDLE
        on_show_art(text, room_name)  → update #art-panel directly
        on_hardware_update(snapshot)  → always refresh #hw-footer
    """

    DEFAULT_CSS = """
    ContextPane {
        width: 40;
        layout: vertical;
        border: none;
    }
    #art-panel {
        display: none;
        height: auto;
        padding: 0 1 1 1;
        color: #27ae60;
    }
    #activity-bar {
        height: 1;
        padding: 0 1;
        color: #607d8b;
    }
    #ctx-header {
        height: 1;
        padding: 0 1;
        color: #4fd1c5;
        background: #0f2a35;
    }
    #ctx-body {
        height: 1fr;
        padding: 0 1;
        overflow-y: auto;
    }
    #hw-footer {
        height: 2;
        padding: 0 1;
        color: #607d8b;
        background: #0f2a35;
    }
    """

    def __init__(self, vocabulary: frozenset[str], **kwargs) -> None:
        """Initialise ContextPane.

        Args:
            vocabulary: Game-dictionary frozenset passed to classify_token()
                        to highlight known Z-machine nouns in teal.
            **kwargs:   Forwarded to Widget.__init__() so that Textual can
                        inject id=, classes=, and other widget parameters.
        """
        super().__init__(**kwargs)
        self._vocabulary = vocabulary
        self._state = _STATE_IDLE

        # Accumulated Rich markup fragments for the THINKING body.
        self._token_buffer: list[str] = []

        # Colorizer context state, reset on each stream.
        self._prev_token = ""
        self._is_sentence_start = True

        # Completed stream log: list of (task_label, rich_markup_text).
        # Newest entries are appended; _render_log() shows them newest-first.
        self._log: list[tuple[str, str]] = []

        # True while we are in post-stream linger (state stays THINKING so
        # hardware polls don't overwrite the content).  Cleared by the app
        # timer calling end_linger(), or cancelled when a new stream starts.
        self._lingering: bool = False

        # Word counter and task label for the activity bar.
        self._word_count: int = 0
        self._activity_task: str = ""

    # ------------------------------------------------------------------
    # Textual compose
    # ------------------------------------------------------------------

    def compose(self) -> ComposeResult:
        """Yield child widgets in display order.

        #art-panel   — persistent ASCII art (top); hidden until first ShowArt.
        #activity-bar— dim single-line LLM activity indicator.
        #ctx-header  — teal state label (THINKING / DONE / LOG).
        #ctx-body    — scrollable token / log content.
        #hw-footer   — pinned hardware telemetry (always visible at bottom).
        """
        yield Static("", id="art-panel", markup=True)
        yield Static("", id="activity-bar", markup=True)
        yield Static("", id="ctx-header")
        yield Static("", id="ctx-body", markup=True)
        yield Static("", id="hw-footer", markup=True)

    # ------------------------------------------------------------------
    # Public methods called by the app's event handlers
    # ------------------------------------------------------------------

    def on_stream_start(self, task: str) -> None:
        """Transition to THINKING state and clear the info body.

        Also cancels any pending linger from the previous stream so the timer
        (if it fires late) does not interrupt the new stream.  Resets the word
        counter and activity bar for the new task.  When the task is "art",
        the art panel is hidden so it is clear that new art is being generated;
        it will reappear when ShowArt arrives.

        Args:
            task: Arbitrary task label shown in the header and activity bar
                  (e.g. "remix", "art", "persona").
        """
        self._lingering = False
        self._state = _STATE_THINKING
        self._token_buffer = []
        self._prev_token = ""
        self._is_sentence_start = True
        self._word_count = 0
        self._activity_task = task
        self._set_header(f"[{_COLOR_HEADER_THINKING}]THINKING  [{task}][/]")
        self._set_body("")
        self._set_activity(f"↓  {task}")
        if task == "art":
            self.query_one("#art-panel", Static).display = False

    def on_token(self, text: str) -> None:
        """Append a streamed token chunk to the THINKING display.

        Each space-delimited word in *text* is classified with classify_token()
        and wrapped in a Rich color tag before being appended to the body.
        Non-empty words also increment the word counter so the activity bar
        can report how many words have been streamed so far.

        Args:
            text: One or more words (possibly with embedded spaces) received
                  from the LLM stream.
        """
        if self._state != _STATE_THINKING:
            return

        for word in text.split(" "):
            if not word:
                # Preserve spacing between tokens.
                self._token_buffer.append(" ")
                continue

            color = classify_token(
                word,
                self._vocabulary,
                prev_token=self._prev_token,
                is_sentence_start=self._is_sentence_start,
            )
            self._token_buffer.append(f"[{color}]{word}[/] ")
            self._word_count += 1

            # Advance colorizer context.
            self._prev_token = word.strip(".,!?;:'\"-()[]").lower()
            # A token ending in sentence-terminal punctuation starts a new
            # sentence for the next word.
            self._is_sentence_start = word.rstrip().endswith((".", "!", "?"))

        self._set_body("".join(self._token_buffer))
        # Update activity bar with running word count.
        self._set_activity(
            f"↓  {self._activity_task}  ·  {self._word_count} words"
        )

    def on_stream_done(self, task: str) -> None:
        """Exit THINKING state.

        All tasks are treated uniformly: if tokens were received, saves them
        to the log and enters a linger period (state stays THINKING so hardware
        polls don't overwrite the content); the app's set_timer() calls
        end_linger() after a delay.  If no tokens were received (e.g. "persona"
        uses call_llm, not call_llm_stream), transitions immediately to IDLE.

        The activity bar is updated to show a checkmark + final word count when
        tokens were received; cleared immediately for zero-token tasks.

        Art is no longer a special case here — the art panel (top) is updated
        separately by on_show_art() and is independent of the info panel state.

        Args:
            task: The task label that was passed to on_stream_start().
        """
        if self._token_buffer:
            # Save completed stream to the log (Rich markup preserved).
            self._log.append((task, "".join(self._token_buffer)))
            # Stay in THINKING so hardware polls don't wipe the content.
            # Header changes to DONE to signal the stream has finished.
            self._lingering = True
            self._set_header(f"[{_COLOR_HEADER_THINKING}]DONE  [{task}][/]")
            # Activity bar: checkmark + final word count.
            self._set_activity(
                f"✓  {task}  ·  {self._word_count} words"
            )
        else:
            # No tokens (persona uses call_llm, not call_llm_stream).
            self._state = _STATE_IDLE
            self._set_header("")
            self._set_activity("")

    def end_linger(self) -> None:
        """Transition from post-stream linger back to IDLE.

        Called by the app's set_timer() callback.  Guards against firing after
        a new stream has already started (on_stream_start clears _lingering).
        Also clears the activity bar when returning to idle state.
        """
        if self._state == _STATE_THINKING and self._lingering:
            self._lingering = False
            self._state = _STATE_IDLE
            self._set_header("")
            self._set_activity("")

    def toggle_log(self) -> None:
        """Toggle the LOG view on/off (bound to F2 in the app).

        Entering LOG: switches state to LOG and renders all accumulated
        stream entries newest-first with task-label separators.
        Exiting LOG: returns to IDLE (safe default regardless of what state
        was active before the log was opened).  Also clears the activity bar
        and header when exiting back to idle.
        """
        if self._state == _STATE_LOG:
            self._lingering = False
            self._state = _STATE_IDLE
            self._set_header("")
            self._set_body("")
            self._set_activity("")
        else:
            self._state = _STATE_LOG
            count = len(self._log)
            self._set_header(f"[#ec96b8]LOG  [{count} entries][/]")
            self._set_body(self._render_log())

    def on_show_art(self, text: str, room_name: str) -> None:
        """Display ASCII art in the persistent art panel (top of ContextPane).

        This does not change the info panel state — THINKING / IDLE / LOG
        continue uninterrupted below.  The art panel persists until the next
        room entry triggers a new on_stream_start("art") call.

        Args:
            text:      Multi-line framed ASCII art string.  Square brackets are
                       escaped so Rich does not mis-parse box-drawing characters
                       as markup tags.
            room_name: Unused here (room name is already embedded in the frame
                       by _frame() in ascii_artist.py); kept for API stability.
        """
        panel = self.query_one("#art-panel", Static)
        safe = text.replace("[", "\\[")
        panel.update(safe)
        panel.display = True

    def on_hardware_update(self, snapshot) -> None:
        """Refresh the pinned #hw-footer from a HardwareSnapshot.

        Unlike the old implementation, this method is NEVER gated by state.
        Hardware telemetry is always displayed in the footer regardless of
        whether the info panel is in THINKING, LOG, or IDLE state.  This
        ensures the footer remains useful throughout the entire session.

        The art panel is unaffected by hardware updates.

        Args:
            snapshot: A HardwareSnapshot namedtuple (from tui/hw_poller.py)
                      with fields tensix_pct, riscv_pct, dram_read_gbps,
                      dram_write_gbps, temp_c, power_w.  Fields set to -1
                      indicate the value is unavailable.
        """
        self.query_one("#hw-footer", Static).update(_render_hardware(snapshot))

    # ------------------------------------------------------------------
    # Private helpers
    # ------------------------------------------------------------------

    def _set_header(self, text: str) -> None:
        """Update the header Static widget with Rich-markup text."""
        self.query_one("#ctx-header", Static).update(text)

    def _set_body(self, text: str) -> None:
        """Update the body Static widget with Rich-markup text."""
        self.query_one("#ctx-body", Static).update(text)

    def _set_activity(self, text: str) -> None:
        """Update the activity bar Static widget with plain text."""
        self.query_one("#activity-bar", Static).update(text)

    def _render_log(self) -> str:
        """Format all log entries as Rich markup, newest entry first.

        Each entry is preceded by a dim separator line showing the task label
        and its position in the log.  The colored token markup from the
        original THINKING stream is preserved so the log looks identical to
        what was displayed live.
        """
        if not self._log:
            return "[#607d8b]no entries yet[/]"
        parts: list[str] = []
        for i, (task, text) in enumerate(reversed(self._log), 1):
            idx = len(self._log) - i + 1
            parts.append(f"[#607d8b]── {idx}: {task} ──[/]\n{text}\n")
        return "\n".join(parts)


# ---------------------------------------------------------------------------
# Hardware rendering helpers
# ---------------------------------------------------------------------------


def _render_hardware(snap) -> str:
    """Format a HardwareSnapshot as a compact one-line Rich string for the footer.

    Returns plain "no hardware" text when all numeric fields are -1 (i.e. the
    poller is running in simulation mode without a real device).

    The footer is only 2 lines tall, so we keep this intentionally compact —
    just temp and power as plain numbers, no progress bars or wide graphs.

    Args:
        snap: HardwareSnapshot with numeric fields; -1 means unavailable.

    Returns:
        Rich markup string suitable for passing to Static.update().
    """
    if all(
        getattr(snap, f) == -1
        for f in ("tensix_pct", "riscv_pct", "dram_read_gbps", "dram_write_gbps", "temp_c", "power_w")
    ):
        return "no hardware"

    parts: list[str] = []

    if snap.temp_c != -1:
        parts.append(f"temp  {snap.temp_c:.0f}°C")
    if snap.power_w != -1:
        parts.append(f"power  {snap.power_w:.0f}W")

    return "    ".join(parts) if parts else "no hardware"
