# Zork on Tenstorrent — Demo Arc Design
**Date:** 2026-04-22  
**Audience:** Tenstorrent engineers + technical blog/asciinema recording  
**Goal:** Ship a four-stage demo that teaches anyone to run Zork on QB2 Blackhole hardware, ending with an LLM remix layer that makes the game infinitely creative.

---

## The Arc

Each stage is independently runnable. The demo and blog post visit them in order. No skipping.

```
Stage 1 — "It runs"
  play.py --stage sim
  Pure Python Z-machine. No hardware. zork_ttlang.py under the hood.
  Story: A 1977 game running in 2026 Python. Nothing special yet.

Stage 2 — "It runs on the hardware"
  play.py --stage device
  Game binary lives in QB2 DRAM as ttnn tensors. zork_device.py under the hood.
  Story: Same interpreter. The data is on a Tenstorrent chip. We did this because we can.

Stage 3 — "The hardware runs it"
  play.py --stage risc-v
  Z-machine interpreter executes on RISC-V cores via TT-Lang / ttnn.generic_op.
  Story: The chip is thinking now, not just storing. Because the hardware is capable of it.

Stage 4 — "Now let's make it fun"
  play.py --stage sim --remix       (or device or risc-v — remix wraps any stage)
  Z-machine + LLM remix layer. ASCII scene art. Narrative postcards. Never told NO again.
  Story: We threw away none of the above. We added a voice. The world bends to you.
```

---

## Architecture

```
play.py --stage [sim|device|risc-v] [--remix] [--persona NAME]

         ┌─────────────────────────────────────────┐
         │              game loop                   │
         │   input → engine → [remix?] → display   │
         └───────┬─────────────┬────────────────────┘
                 │             │
         ┌───────▼──────┐  ┌───▼────────────────────────────┐
         │  Z-machine   │  │        LLM Remix Layer          │
         │   Engine     │  │    (inactive in classic mode)   │
         │              │  │                                 │
         │  Stage 1:    │  │  input_mapper                   │
         │  zmachine    │  │    "open mailbox with teeth"    │
         │  _v3.py      │  │    → "open mailbox"             │
         │              │  │                                 │
         │  Stage 2:    │  │  output_remixer                 │
         │  zork_       │  │    (user_input, zork_response)  │
         │  device.py   │  │    → remixed_response           │
         │              │  │                                 │
         │  Stage 3:    │  │  ascii_artist                   │
         │  zork_       │  │    (room/action) → ASCII art    │
         │  risc.py     │  │                                 │
         └───────┬──────┘  │  narrative_enhancer             │
                 │         │    postcards at game end        │
                 └────┬────┘                                 │
                      │    personas, mode_switcher           │
                      ▼    └─────────────────────────────────┘
                  terminal
```

**The remix layer is a wrapper, not a replacement.** Z-machine runs every turn regardless of mode. In classic mode, output passes straight to terminal. In remix mode, every turn goes through `(user_input, zork_output) → LLM → display`.

**In-game mode toggle:** player types `/classic` or `/remix` to switch live. No restart.

---

## Components

### play.py (new — unified entry point)
- `--stage sim|device|risc-v` selects the engine
- `--remix` enables the LLM layer at startup (player can also toggle in-game)
- `--persona NAME` selects an AI player persona for auto-play
- `--model MODEL` overrides the LLM model (default: local Ollama)
- Shared game loop: `while True: input → engine.step() → [remix] → print`
- `/classic`, `/remix`, `/help`, `/quit` as in-game commands
- Stage label printed at start so recordings are self-documenting

### engines/ (thin wrappers around existing runners)
- `engines/sim.py` — wraps `ZMachineV3` from `ttlang/zmachine_v3.py`
- `engines/device.py` — wraps `zork_device.py` QB2 DRAM runner
- `engines/riscv.py` — wraps `zork_risc.py` RISC-V kernel runner
- Common interface: `engine.step(command: str) -> str`

### remix/ (new Python package)
- `remix/input_mapper.py` — maps freeform input to valid Zork command via LLM
- `remix/output_remixer.py` — rewrites Z-machine response to match user's phrasing
- `remix/ascii_artist.py` — generates ASCII scene art for room descriptions
- `remix/narrative_enhancer.py` — ported from C++ branch, generates postcards
- `remix/mode.py` — tracks current mode (classic/remix), handles toggle commands

### LLM backend
- Default: local Ollama (`qwen2.5:1.5b` for mapping, `qwen2.5:7b` for remixing/art)
- Configurable via `--model` or `ZORK_LLM_MODEL` env var
- Graceful fallback: if LLM unavailable, remix layer passes through unchanged
- LLM router from `feature/multi-llm-integration` adapted for Python

---

## ASCII Scene Art

The `ascii_artist` module is called once per room entry (not every turn — expensive). It generates a small ASCII illustration of the current room using the room name and description as prompt. Displayed above the room text:

```
╔══════════════════════════╗
║   West of House          ║
║    _____                 ║
║   |     |  __            ║
║   | [=] | /  \  ~  ~     ║
║   |_____|/    \  ~ ~     ║
║   /\/\/\/\/\/\/\/\/\/\   ║
╚══════════════════════════╝

You are standing in an open field west of a white house,
with a boarded front door. There is a small mailbox here.
```

Art is cached per room — generated once, reused on revisit. Max 6 lines tall to fit terminal recordings cleanly.

---

## Narrative Postcards (from multi-llm branch)

At game end (death or victory), the existing ASCII journey map is displayed, followed by "postcards" — short LLM-generated descriptions of memorable moments:

```
╔══════════════════════════════════════╗
║  POSTCARDS FROM YOUR JOURNEY        ║
╠══════════════════════════════════════╣
║ West of House — "You arrived at the ║
║ white house as dusk fell, mailbox   ║
║ gleaming like a dare..."            ║
╚══════════════════════════════════════╝
```

Triggered on: first location, first death, finding the brass lantern, winning.

---

## What We Integrate from feature/multi-llm-integration

| Component | Action |
|-----------|--------|
| `src/llm/narrative_enhancer.h` | Port to `remix/narrative_enhancer.py` |
| Player personas (4 personalities) | Port to `remix/personas.py`, use with `--persona` |
| `scripts/switch-mode.sh` logic | Inline into `remix/mode.py` |
| LLM router (cheap/rich model split) | Adapt to Python in `remix/router.py` |
| `scripts/start-single-8b.sh` | Keep as-is, document in README |

**Left out (Phase 2):** ncurses split-screen TUI, SDXL image generation, multi-chip tensor parallel, full A/B test infrastructure.

---

## Demo Script (asciinema)

Four recordings, each ~2 minutes:

1. `demo-stage1.sh` — sim mode, show opening + `open mailbox with my teeth`
2. `demo-stage2.sh` — device mode, show QB2 DRAM upload + same gameplay
3. `demo-stage3.sh` — risc-v mode, show RISC-V kernel execution + output
4. `demo-stage4.sh` — remix mode live toggle: `/classic` then `/remix`, same mailbox command, side-by-side contrast

Each script: title card → `play.py --stage N` → scripted input → exit.

---

## Blog Post Structure

1. **"Why Zork?"** — history, Z-machine, why it's a perfect first program for new hardware
2. **Stage 1** — embed demo-stage1.sh recording, explain ZMachineV3
3. **Stage 2** — embed demo-stage2.sh, explain ttnn DRAM tensors, page_size gotcha
4. **Stage 3** — embed demo-stage3.sh, explain TT-Lang, RISC-V cores, firmware watchdog batching
5. **Stage 4** — embed demo-stage4.sh, show the teeth example, explain remix architecture
6. **"Try it yourself"** — `git clone`, `pip install`, `python play.py --stage sim`

---

## Out of Scope

- SDXL image generation
- ncurses split-screen TUI
- Multi-chip tensor parallel models
- Multiplayer / networked play
- Saving/restoring game state across sessions (existing limitation)

---

## Success Criteria

- `python play.py --stage sim` runs interactively with no hardware
- `python play.py --stage device` runs with QB2 DRAM tensors
- `python play.py --stage risc-v` shows RISC-V output (opening text at minimum)
- `python play.py --stage sim --remix` lets user type anything, never says NO
- `/classic` and `/remix` toggle live mid-game
- ASCII room art renders for at least the first 5 rooms
- Postcards display on game end
- Four asciinema recordings exist and play back cleanly
- Blog post draft covers all four stages
