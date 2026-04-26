# tui/game_pane.py
"""GamePane — left panel of the TUI: scrolling game text and player input.

Receives GameText messages from the game thread via the app's event loop.
The Input widget at the bottom captures player commands; Input.Submitted
events bubble up to ZMachineTuiApp for queuing.
"""
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
        """Append Z-machine or remixed output, coloured #e8f0f2 (off-white).

        Appends a blank line after the content block to visually separate turns.
        """
        log = self.query_one("#game-log", RichLog)
        for line in text.rstrip().splitlines():
            log.write(f"[#e8f0f2]{line}[/]")
        log.write("")  # blank line between turns

    def write_player_text(self, text: str) -> None:
        """Append player input echo in dim grey."""
        log = self.query_one("#game-log", RichLog)
        for line in text.rstrip().splitlines():
            log.write(f"[#607d8b]{line}[/]")
