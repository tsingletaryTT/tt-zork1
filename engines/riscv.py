# SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
# SPDX-License-Identifier: Apache-2.0
"""
engines/riscv.py — Stage 3: Z-machine interpreter running on QB2 RISC-V cores.

Wraps ttlang/zork_risc.py which dispatches kernels/zork_interpreter_l1.cpp via
ttnn.generic_op with per-batch device sessions (10 instructions per kernel
invocation — the only count confirmed safe on QB2 Blackhole even when PRINT fires).
State persists across batches via host-side bytes serialised between device sessions.

Stage overview (from docs/implementation-plan.md):
    Stage 1 — SimEngine:    Pure Python Z-machine, everything on host CPU.
    Stage 2 — DeviceEngine: Pure Python Z-machine, game binary on QB2 DRAM.
    Stage 3 — RiscVEngine:  Python host, Z-machine interpreter on QB2 RISC-V cores. ← HERE
    Stage 4 — RemixLayer:   Stage 3 + LLM remix on Tensix cores (future).

Batched execution model:
    The QB2 firmware watchdog limits each kernel invocation. interpret(10) is the
    only count confirmed safe even when PRINT fires and triggers Z-string decode
    overhead within the watchdog window. interpret(20+) hangs at the batch where
    PRINT fires. Producing the Zork opening text requires ~100 instructions
    (STARTUP_BATCHES=10 × 10 = 100). A typical command takes ~60 instructions
    (STEP_BATCHES=6 × 10 = 60).

    Additionally the third ttnn.generic_op() call within a single open_device()
    session always hangs (confirmed by diag_batch3.py). run_zork() works around
    this by opening and closing the ttnn device for EACH batch, serialising
    ZMachineState to host-side bytes between sessions.

    run_zork() (in ttlang/zork_risc.py) handles the batch loop transparently:
        1. For each batch: open QB2 device via ttnn.open_device(device_id=0)
        2. Allocates DRAM tensors: game, output, input, state (restored or fresh)
        3. Runs the RISC-V kernel once per device session
        4. Downloads state to host bytes, closes device
        5. Repeats N times, accumulating per-batch output text

TT-Lang pyenv requirement:
    The ttnn package is only available after running:
        source ~/code/tt-lang/build/env/activate
    If ttlang.zork_risc is not importable, _RISCV_AVAILABLE is set to False and
    RiscVEngine.__init__ raises ImportError with instructions.
"""
from __future__ import annotations

from pathlib import Path

from engines.base import BaseEngine

# ---------------------------------------------------------------------------
# Lazy import guard — TT-Lang pyenv must be active for ttnn to be importable.
# If zork_risc.py is unavailable (wrong venv, or file not yet in worktree),
# _RISCV_AVAILABLE is False and RiscVEngine.__init__ raises ImportError rather
# than crashing at module import time.
# ---------------------------------------------------------------------------
try:
    from ttlang.zork_risc import run_zork  # type: ignore[import]
    _RISCV_AVAILABLE = True
except ImportError:
    _RISCV_AVAILABLE = False

# ---------------------------------------------------------------------------
# Batch constants
# ---------------------------------------------------------------------------

# 10 batches × 10 instructions = 100 total.
# The Zork opening sequence ("ZORK I: The Great Underground Empire...") requires
# roughly 80–120 Z-machine instructions; 100 covers the full opening text including
# the "West of House" room description. interpret(10) is the only confirmed-safe
# limit — interpret(20+) hangs at the batch where PRINT fires (Z-string decode
# overhead tips the QB2 firmware watchdog).
STARTUP_BATCHES = 10

# 6 batches × 10 instructions = 60 total.
# Most single Zork commands (look, open mailbox, go north, etc.) complete in
# 30–60 instructions. 6 batches × 10 = 60 covers typical commands.
STEP_BATCHES = 6


class RiscVEngine(BaseEngine):
    """Z-machine interpreter engine running on QB2 Blackhole RISC-V cores.

    This is Stage 3 of the Zork demo arc. Unlike SimEngine and DeviceEngine,
    the Z-machine interpreter itself executes on the Tenstorrent RISC-V cores
    rather than on the host CPU. The Python host only orchestrates batch
    invocations and reads the output text from DRAM.

    The underlying kernel (kernels/zork_interpreter_l1.cpp) implements a full
    Z-machine V3 interpreter with 24+ opcodes, abbreviation decoding, object
    property access, and state persistence. Each kernel invocation executes 10
    Z-machine instructions — the only count confirmed safe even when PRINT fires
    and triggers Z-string decode overhead within the firmware watchdog window.

    State persistence between commands:
        run_zork() allocates a fresh state DRAM tensor per call, so each call
        to startup() or step() restarts the Z-machine from scratch. This matches
        the current batched architecture where state is not shared across Python
        calls. For a persistent game session, run_zork() would need to be
        extended to accept an external state tensor — that is a future Stage 3+
        enhancement.

    Hardware requirement:
        - Tenstorrent QB2 Blackhole hardware (accessible via /dev/tenstorrent)
        - TT-Lang pyenv active: source ~/code/tt-lang/build/env/activate
        - ttlang/zork_risc.py present in the module search path
        - kernels/zork_interpreter_l1.cpp present in the repo
    """

    label = "Stage 3 — Z-machine interpreter on QB2 RISC-V cores (TT-Lang)"

    def __init__(self, game_path: str) -> None:
        """Initialise the RiscVEngine.

        Args:
            game_path: Absolute or relative path to the Z-machine V3 game file
                       (e.g., "game/zork1.z3"). The file must exist.

        Raises:
            ImportError:     TT-Lang pyenv not active (ttlang.zork_risc not importable).
            FileNotFoundError: game_path does not exist.
        """
        super().__init__(game_path)
        if not _RISCV_AVAILABLE:
            raise ImportError(
                "ttlang.zork_risc is not importable. "
                "Activate the TT-Lang pyenv first:\n"
                "    source ~/code/tt-lang/build/env/activate"
            )
        if not Path(game_path).exists():
            raise FileNotFoundError(game_path)

    # ------------------------------------------------------------------
    # BaseEngine interface
    # ------------------------------------------------------------------

    def startup(self) -> str:
        """Run the Zork opening sequence on QB2 RISC-V.

        Executes STARTUP_BATCHES (10) kernel invocations × 10 instructions each,
        for 100 total Z-machine instructions. This is sufficient to produce the
        full Zork opening text including copyright notice, initial room
        description ("West of House"), and the first command prompt.

        Returns:
            Game text output from the opening sequence (ASCII string).
        """
        return run_zork(
            self.game_path,
            command="",
            verbose=False,
            num_batches=STARTUP_BATCHES,
        )

    def step(self, command: str) -> str:
        """Execute one Zork command on QB2 RISC-V and return the response.

        Executes STEP_BATCHES (6) kernel invocations × 10 instructions each,
        for 60 total Z-machine instructions. Most single commands complete in
        30–60 instructions, so this covers the majority of Zork inputs.

        Note: Each call to step() opens a fresh QB2 device connection and
        initialises the Z-machine from scratch (run_zork allocates a new state
        tensor per call). This means the game state does NOT persist between
        step() calls in the current implementation. For a true interactive loop,
        integrate an external state tensor — a future Stage 3+ enhancement.

        Args:
            command: Zork input command (e.g., "open mailbox", "go north").
                     Empty string runs the opening sequence without input.

        Returns:
            Game text output for this command (ASCII string).
        """
        return run_zork(
            self.game_path,
            command=command,
            verbose=False,
            num_batches=STEP_BATCHES,
        )

    @property
    def game_over(self) -> bool:
        """Always False — RiscVEngine does not track game state between calls.

        The RISC-V kernel saves ZMachineState to DRAM within a single run_zork()
        call, but that state is not returned to the Python layer. The host loop
        should detect game termination by watching for empty output (the kernel
        outputs nothing once the game finishes and interpreter halts).
        """
        return False

    @property
    def running(self) -> bool:
        """Always True — engine is stateless between calls; no lifecycle tracking."""
        return True

    def close(self) -> None:
        """No-op — run_zork opens and closes the QB2 device internally per call.

        The ttnn.open_device / ttnn.close_device pair lives inside run_zork()
        (in ttlang/zork_risc.py). Each startup() / step() call is fully
        self-contained. There are no persistent device handles to release here.
        """
        pass
