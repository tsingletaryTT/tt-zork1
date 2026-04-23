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
