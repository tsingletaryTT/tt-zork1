#!/usr/bin/env python3
# play.py — Infocom on Tenstorrent: unified demo entry point
#
# Usage (no args → interactive menu):
#   python play.py
#
# Direct launch (skips menu):
#   python play.py --game game/zork1.z3 --stage sim
#   python play.py --game game/zork1.z3 --stage sim --remix --tui
#   python play.py --game game/zork1.z3 --stage sim --remix --persona expert
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Game catalogue
# ---------------------------------------------------------------------------

GAMES: list[dict] = [
    {
        "name":    "Zork I: The Great Underground Empire",
        "file":    "game/zork1.z3",
        "version": "Z3 · MIT License",
        "year":    "1980",
    },
    {
        "name":    "Zork II: The Wizard of Frobozz",
        "file":    "game/zork2.z3",
        "version": "Z3 · MIT License",
        "year":    "1981",
    },
    {
        "name":    "Zork III: The Dungeon Master",
        "file":    "game/zork3.z3",
        "version": "Z3 · MIT License",
        "year":    "1982",
    },
    {
        "name":    "Adventure (Colossal Cave, 350pt)",
        "file":    "game/advent.z5",
        "version": "Z5 · BSD 2-clause",
        "year":    "1977",
    },
]

STAGE_LABELS = {
    "sim":    "pure Python Z-machine  (no hardware)",
    "device": "game data on QB2 DRAM  (Tenstorrent silicon)",
    "risc-v": "interpreter on RISC-V cores  (TT-Lang)",
}

BANNER = """\
╔══════════════════════════════════════════════════════════════
║      I N F O C O M   on   T E N S T O R R E N T
║  classic interactive fiction · 2026 AI accelerator hardware
╚══════════════════════════════════════════════════════════════"""

HELP_TEXT = """\
Commands: /classic  /remix  /help  /quit
  /classic  — disable LLM remix layer (raw Z-machine output)
  /remix    — enable  LLM remix layer (creative responses)
  /help     — show this message
  /quit     — exit the game"""


# ---------------------------------------------------------------------------
# Interactive startup menu
# ---------------------------------------------------------------------------

def _on(flag: bool) -> str:
    return "\033[32mon \033[0m" if flag else "\033[90moff\033[0m"


def _stage_label(s: str) -> str:
    labels = {"sim": "Sim  ", "device": "Device", "risc-v": "RISC-V"}
    return labels.get(s, s)


def _print_menu(game_idx: int, stage: str, remix: bool, tui: bool) -> None:
    print("\033[2J\033[H", end="")  # clear screen
    print(BANNER)
    print()
    print("  \033[90m── Games ───────────────────────────────────────────────────\033[0m")
    for i, g in enumerate(GAMES, 1):
        marker = "\033[36m●\033[0m" if (i - 1) == game_idx else " "
        avail  = Path(g["file"]).exists()
        dim    = "" if avail else "\033[90m"
        reset  = "\033[0m"
        note   = f"  \033[90m{g['version']} · {g['year']}\033[0m" if avail else "  \033[90m[file not found]\033[0m"
        print(f"  {marker} {dim}{i}  {g['name']}{reset}{note}")
    print()
    print("  \033[90m── Stage ───────────────────────────────────────────────────\033[0m")
    for key, label in STAGE_LABELS.items():
        marker = "\033[36m●\033[0m" if stage == key else " "
        skey   = {"sim": "s", "device": "d", "risc-v": "r"}[key]
        print(f"  {marker} {skey}  {_stage_label(key)}  \033[90m— {label}\033[0m")
    print()
    print("  \033[90m── Options ─────────────────────────────────────────────────\033[0m")
    print(f"  {'●' if tui   else ' '} t  TUI    — Textual side panel      [{_on(tui)}]")
    print(f"  {'●' if remix else ' '} x  Remix  — LLM remix layer         [{_on(remix)}]")
    print()
    print("  \033[90m1-4 pick game · s/d/r set stage · t/x toggle · Enter play · q quit\033[0m")


def interactive_menu(
    game_idx: int = 0,
    stage: str = "sim",
    remix: bool = False,
    tui: bool = False,
) -> argparse.Namespace:
    """Show the startup menu and return selections as a Namespace.

    Pre-seeds the displayed state from CLI args so that flags like --remix
    and --tui are already toggled on when the menu opens.
    """

    while True:
        _print_menu(game_idx, stage, remix, tui)
        try:
            raw = input("  > ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            sys.exit(0)

        if raw in ("1", "2", "3", "4"):
            idx = int(raw) - 1
            if Path(GAMES[idx]["file"]).exists():
                game_idx = idx
            else:
                input(f"  \033[91mFile not found: {GAMES[idx]['file']}\033[0m  (press Enter)")
        elif raw == "s":
            stage = "sim"
        elif raw == "d":
            stage = "device"
        elif raw == "r":
            stage = "risc-v"
        elif raw == "t":
            tui = not tui
        elif raw == "x":
            remix = not remix
        elif raw in ("q", "quit"):
            sys.exit(0)
        elif raw == "":
            break

    ns = argparse.Namespace(
        game    = GAMES[game_idx]["file"],
        stage   = stage,
        remix   = remix,
        tui     = tui,
        persona = None,
        turns   = 50,
        model   = None,
    )
    return ns


# ---------------------------------------------------------------------------
# Engine / loop helpers
# ---------------------------------------------------------------------------

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


def persona_loop(engine, remix_layer, persona: dict, max_turns: int = 50) -> None:
    """Drive the game automatically using an LLM persona to choose each command."""
    from remix.llm import call_llm
    from remix.router import route

    out = engine.startup()
    _display(out, remix_layer, user_input=None, is_startup=True)

    for _turn in range(max_turns):
        cmd = call_llm(
            system=persona["system_prompt"],
            user=out,
            model=route("persona"),
            temperature=0.7,
        )
        if not cmd or not cmd.strip():
            print(f"\n[{persona['name']}: LLM returned no command — stopping]")
            break
        cmd = cmd.strip()
        print(f"\n[{persona['name']}] > {cmd}")

        out = engine.step(cmd)
        _display(out, remix_layer, user_input=cmd)

        if getattr(engine, "game_over", False):
            if remix_layer:
                remix_layer.on_game_end()
            return
    else:
        print(f"\n[{persona['name']}: reached {max_turns}-turn limit]")
        if remix_layer:
            remix_layer.on_game_end()


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


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Infocom on Tenstorrent — classic text adventure on AI hardware",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Run with no arguments for the interactive startup menu.",
    )
    parser.add_argument("--game", default=None,
                        help="Path to Z-machine story file (skips menu)")
    parser.add_argument("--stage", choices=["sim", "device", "risc-v"],
                        default=None, help="Engine backend (skips menu)")
    parser.add_argument("--remix", action="store_true",
                        help="Enable LLM remix layer at startup")
    parser.add_argument("--model", default=None,
                        help="Override LLM model for all remix calls")
    parser.add_argument("--persona", default=None,
                        help="Auto-play persona (expert/naive/completionist/experimental)")
    parser.add_argument("--turns", type=int, default=50,
                        help="Max turns in persona auto-play mode (default: 50)")
    parser.add_argument("--tui", action="store_true",
                        help="Launch Textual TUI (side panel with art, LLM stream, hardware)")
    parser.add_argument("--menu", action="store_true",
                        help="Force the interactive startup menu even with other flags")
    args = parser.parse_args()

    # Show interactive menu when no game/stage given, or when --menu is forced.
    # Pre-seed the menu with whatever flags were already supplied on the CLI
    # so that --remix, --tui, --game, --stage arrive as the initial selection.
    if args.menu or (args.game is None and args.stage is None):
        initial_game_idx = 0
        if args.game:
            for i, g in enumerate(GAMES):
                if g["file"] == args.game:
                    initial_game_idx = i
                    break
        args = interactive_menu(
            game_idx = initial_game_idx,
            stage    = args.stage or "sim",
            remix    = bool(args.remix),
            tui      = bool(args.tui),
        )
    else:
        # Fill in defaults for anything not supplied on the CLI.
        if args.game is None:
            args.game = GAMES[0]["file"]
        if args.stage is None:
            args.stage = "sim"

    if not Path(args.game).exists():
        print(f"Error: game file not found: {args.game}", file=sys.stderr)
        sys.exit(1)

    if args.model:
        os.environ["ZORK_LLM_MODEL"] = args.model

    # Derive a short title for the banner from the catalogue, or use the filename.
    game_title = next(
        (g["name"] for g in GAMES if g["file"] == args.game),
        Path(args.game).stem,
    )
    print(BANNER)
    print(f"\n  {game_title}")
    print(f"  {STAGE_LABELS.get(args.stage, args.stage)}\n")

    remix_layer = None
    if args.remix or args.persona:
        from remix.mode import RemixLayer
        remix_layer = RemixLayer()
        print("  [Remix layer active — type /classic to disable]\n")

    engine = build_engine(args.stage, args.game)
    try:
        if args.tui:
            try:
                from tui.app import ZMachineTuiApp
            except ImportError:
                print("Error: 'textual' not installed. Run: pip install textual",
                      file=sys.stderr)
                sys.exit(1)
            ZMachineTuiApp(
                engine=engine,
                remix_layer=remix_layer,
                game_path=args.game,
                initial_persona=args.persona,
                initial_turns=args.turns,
            ).run()
        elif args.persona:
            from remix.personas import get_persona
            persona = get_persona(args.persona)
            print(f"  [Auto-play: {persona['name']} · {args.turns}-turn limit]\n")
            persona_loop(engine, remix_layer, persona, max_turns=args.turns)
        else:
            game_loop(engine, remix_layer)
    finally:
        engine.close()


if __name__ == "__main__":
    main()
