# We Put Zork on a Tenstorrent AI Chip (And Then Made It Weirder)

*A story about Z-machines, RISC-V cores, firmware watchdogs, and when to declare victory anyway.*

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
python play.py --stage sim
```

[embed: stage1.cast]

We wrote a pure Python Z-machine V3 interpreter (`ttlang/zmachine_v3.py`).
Nothing remarkable here — it decodes Z-strings, manages a call stack,
handles the object tree. About 700 lines. It runs Zork I end-to-end.

This is Stage 1. No hardware. Just proof that we understand the format.

---

## Stage 2 — It Runs on the Hardware

```bash
python play.py --stage device
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

## Stage 3 — The Hardware Runs It (Mostly)

Now we move the interpreter itself onto the chip. The Blackhole's RISC-V
cores can run arbitrary C++ via TT-Lang's `ttnn.generic_op` +
`KernelDescriptor` API. We compiled our Z-machine interpreter
(`kernels/zork_interpreter_l1.cpp`) as a RISC-V kernel and dispatched it
to core (0,0).

The hardware is thinking now.

### What we proved

We got the Zork opening text out of a RISC-V core. That text is real.
The interpreter fetches 87 KB of game data from DRAM over the NoC, decodes
Z-strings, manages a call stack, evaluates conditional branches — the whole
thing — inside a RISC-V processor embedded in an AI accelerator.

```
ZORK I: The Great Underground Empire
Infocom interactive fiction - a fantasy story
Copyright (c) 1981, 1982, 1983 Infocom, Inc. All rights reserved.
```

We are unreasonably proud of that.

### Where we got stuck

Two hard constraints appeared on top of each other.

**Constraint 1: firmware watchdog.** The QB2 firmware kills kernels that
run too long. Through binary search we found `interpret(10)` — 10 Z-machine
instructions — is the reliable ceiling. `interpret(20)` hangs at any batch
where a `PRINT` opcode fires, because Z-string decoding adds enough overhead
to tip the watchdog. The Zork opening requires ~400 instructions, so we need
40+ batches.

**Constraint 2: the third-invocation hang.** We batched execution by
persisting `ZMachineState` (PC, stack, call frames, dynamic game memory) to
a DRAM tensor and running multiple kernel invocations inside one device
session. Batches 1 and 2 complete reliably. Batch 3 always hangs —
regardless of instruction count, state content, or whether we reset the
state tensor to all zeros. We confirmed this with a diagnostic script
(`ttlang/diag_batch3.py`) that showed even a fresh-init batch 3 hangs.
The firmware or device session state is accumulating something between
`generic_op` calls that we cannot reset from Python.

**Workaround attempt.** We restructured `run_zork()` to open and close the
`ttnn` device for *each* batch, serialising state to host bytes between
sessions. This eliminates the third-invocation hang — every batch is the
first in its device session, so it always completes. But the device
open/close overhead (~0.5s) means 40 batches takes ~20 seconds just in
session setup, plus JIT recompilation on the first batch of each run.

**Where we landed.** Stage 3 is a proof of concept, not a playable game.
The RISC-V interpreter works. The state persistence works. The constraints
are real firmware-level limits on the QB2 platform as we encountered it.
The right next step is a firmware update conversation with the TT-Metal team
— but that is out of scope for this demo.

For the demo, we move on.

---

## Stage 4 — Now Let's Make It Fun

```bash
python play.py --stage sim --remix
```

[embed: stage4.cast]

Stages 1–3 prove the hardware story. Stage 4 is where the demo gets
interesting to play.

The Z-machine runs on the host CPU (Stage 1 path) — fast, reliable, no
firmware constraints. The hardware contribution here is the language model:
an LLM running on the Tensix AI fabric of the same Blackhole chip,
rewriting every game response in real time.

Classic Zork is unforgiving. "Open the mailbox with my teeth" gets you
"I don't understand that." "Fight the darkness with hope" gets you
"That's not a verb I recognise." The parser is the wall.

In remix mode, the Z-machine still runs every turn. It's still the source
of truth for game state — what exists, what you're carrying, what rooms
connect. But we pass every response through an LLM on the **Tensix cores**
and it rewrites the flavour text to match your energy. The LLM receives
what you typed *and* what the game responded, then bends the voice of the
world to meet you.

RISC-V cores demonstrated the interpreter. Tensix cores power the remix.
Both are on Tenstorrent silicon.

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

# Stage 4 — with LLM remix
# Requires tt-inference-server (https://github.com/tenstorrent/tt-inference-server)
# running Llama on your Tenstorrent chip's Tensix cores.
# Default endpoint: http://localhost:8000/v1/chat/completions
# To override (e.g. point at Ollama): export ZORK_LLM_URL=http://localhost:11434/v1/chat/completions
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
