# SPDX-License-Identifier: Apache-2.0
"""
zork_kernel.py — TT-Lang operation scaffolding for on-chip Z-machine execution.

Stage 3 Architecture:
---------------------
This module defines the TT-Lang @ttl.operation that will run the Z-machine
V3 interpreter on QB2 RISC-V cores.  Three threads execute concurrently
on a single Tensix core:

    dm_reader  │  loads game data + state + input from DRAM into L1 DFBs
    compute    │  runs the Z-machine interpreter loop (stubbed → full impl)
    dm_writer  │  flushes interpreter output back to DRAM

Buffer constants (byte sizes, match kernels/zork_interpreter_opt.cpp):
    GAME_SIZE   = 87040   bytes  — zork1.z3 file (exact; already chunk-aligned)
    OUTPUT_SIZE = 16384   bytes  — output ring buffer (text printed by game)
    STATE_SIZE  = 16384   bytes  — interpreter state snapshot (PC, stack, …)
    INPUT_SIZE  =   256   bytes  — null-terminated command string from host

CHUNK_SIZE = 512 bytes.  All DFBs use block_count=2 (double-buffering) so
dm_reader can prefetch the next block while compute processes the current one.

TT-Lang DFB state-machine constraints (enforced by the simulator):
    • A DM thread that calls .wait() on a block MUST copy FROM it (copy_src).
    • A compute thread that calls .wait() on a block MUST use it as a store()
      source (o_blk.store(input_blk)).
    • A compute thread that calls .reserve() on a block MUST store INTO it.
These constraints are satisfied by the scaffolding.  The stub routes
state/input through compute→output as placeholder behaviour; the real
interpreter (zmachine_v3.py logic) replaces those store() calls in Task 10.

This file is the *scaffolding* for Task 9.  The DFB wiring and tensor sizes
are real TT-Lang code that will run in the simulator today.  The interpreter
logic inside compute() is intentionally minimal — it copies the first game
chunk to output — so the pipeline can be validated end-to-end before
integrating the full Z-machine logic in Task 10.

Usage (simulator, no hardware required):
    source ~/code/tt-lang/build/env/activate
    python ttlang/zork_kernel.py [game/zork1.z3]

Usage (device, future Task 10):
    # Open ttnn device, call run_kernel_on_device().
"""
from __future__ import annotations

import sys
from pathlib import Path

# Make the repo root importable regardless of cwd.
sys.path.insert(0, str(Path(__file__).parent.parent))

import torch
from sim import ttl
from sim.ttnnsim import Tensor, ROW_MAJOR_LAYOUT

# ---------------------------------------------------------------------------
# Buffer size constants — must stay in sync with zork_interpreter_opt.cpp
# ---------------------------------------------------------------------------

# Exact size of zork1.z3 (87040 bytes), which is already a multiple of
# CHUNK_SIZE=512, so no padding is needed.
GAME_SIZE: int = 87040

# DFB block size in scalar (float32) elements.  512 bytes fits comfortably in
# L1 (128 KB L1 on QB2; four 512-byte blocks = 2 KB for double-buffered DFBs).
CHUNK_SIZE: int = 512
N_GAME_CHUNKS: int = GAME_SIZE // CHUNK_SIZE   # 170

OUTPUT_SIZE: int = 16384   # 16 KB — maximum output text per kernel invocation
STATE_SIZE: int = 16384    # 16 KB — interpreter state (PC, stack, frames, …)
INPUT_SIZE: int = 256      # command string (null-terminated ASCII)


# ---------------------------------------------------------------------------
# The TT-Lang operation
# ---------------------------------------------------------------------------

@ttl.operation(grid=(1, 1))
def zmachine_kernel(
    game: Tensor,       # [GAME_SIZE]    — Z-machine ROM + dynamic memory (DRAM)
    state: Tensor,      # [STATE_SIZE]   — interpreter state checkpoint (DRAM)
    input_buf: Tensor,  # [INPUT_SIZE]   — current command string from host
    output: Tensor,     # [OUTPUT_SIZE]  — output text written by interpreter
) -> None:
    """
    TT-Lang operation: load Z-machine data into L1, run interpreter, flush output.

    Three threads run cooperatively via the TT-Lang simulator green-thread
    scheduler (on hardware these map to RISC-V DM0, DM1, and the Tensix
    compute engine):

        dm_reader  — DRAM → L1: game + state + input via DFB blocks
        compute    — L1 only:   interpret opcodes (stub: echo first chunk)
        dm_writer  — L1 → DRAM: output text back to device tensor

    DFB sizing:
        game_dfb   — (CHUNK_SIZE,)  × block_count=2  ← double-buffered
        out_dfb    — (CHUNK_SIZE,)  × block_count=2  ← double-buffered
        state_dfb  — (CHUNK_SIZE,)  × block_count=1  ← single transfer
        input_dfb  — (CHUNK_SIZE,)  × block_count=1  ← single transfer

    State-machine notes:
        In the simulator every DFB block acquisition (reserve/wait) must be
        paired with a legally sequenced store()/copy() before the context
        manager exits.  The stub satisfies this by routing state and input
        blocks through output_dfb.reserve() store() calls (marked TODO below).
        When Task 10 replaces the stub, those store() calls become real
        interpreter state-restore and command-parse operations.

    Stub behaviour (Task 9):
        Only the first CHUNK_SIZE bytes of game data are processed.
        dm_reader loads: state[0:CHUNK_SIZE], input[0:CHUNK_SIZE],
                         game[0:CHUNK_SIZE].
        compute:         state → out (scratch), input → out (scratch),
                         game  → out (real output).
        dm_writer:       writes all three out blocks back to output[0:CHUNK_SIZE]
                         (the final write is the real game-header payload).

    Full behaviour (Task 10 — TODO):
        dm_reader loads all N_GAME_CHUNKS=170 game chunks in a loop.
        compute calls ZMachineV3.interpret(N) for each batch, producing
        variable-length output text and an updated state snapshot.
        dm_writer writes output text + updated state to DRAM.
    """

    # ------------------------------------------------------------------
    # Dataflow buffers
    # ------------------------------------------------------------------
    # Double-buffered for game and output so compute and DM overlap:
    # while compute processes block N, dm_reader is prefetching block N+1.
    game_dfb = ttl.make_dataflow_buffer_like(game,     shape=(CHUNK_SIZE,), block_count=2)
    out_dfb  = ttl.make_dataflow_buffer_like(output,   shape=(CHUNK_SIZE,), block_count=2)

    # Single-block buffers for state and input (no pipelining needed; each
    # is transferred once per kernel invocation).
    state_dfb = ttl.make_dataflow_buffer_like(state,     shape=(CHUNK_SIZE,), block_count=1)
    input_dfb = ttl.make_dataflow_buffer_like(input_buf, shape=(CHUNK_SIZE,), block_count=1)

    # ------------------------------------------------------------------
    # Compute thread
    # ------------------------------------------------------------------
    @ttl.compute()
    def compute() -> None:
        """
        Interpreter stub — demonstrates the three-way DFB handshake.

        The TT-Lang state machine requires that every block acquired via
        .wait() be used as a store() source before the context manager exits.
        The stub satisfies this requirement with three store() calls:

            1.  state_blk  → out_blk  (scratch write; TODO: restore state)
            2.  input_blk  → out_blk  (scratch write; TODO: parse command)
            3.  game_blk   → out_blk  (real output:  first 512 game bytes)

        The dm_writer thread writes all three output blocks back to DRAM;
        only the last one carries meaningful payload.

        Full implementation (Task 10):
            Replace the three stub store() calls with:
              1. state_blk → unpack PC, stack frames into ZMachineV3 fields
              2. input_blk → set ZMachineV3.input_command
              3. Loop over game chunks, run ZMachineV3.interpret(BATCH_SIZE),
                 collect output into out_blk via ZMachineV3.flush_output()
        """
        # Step 1: Load interpreter state (stub: pass through to output).
        # TODO (Task 10): unpack ZMachineV3 state from state_blk bytes.
        with state_dfb.wait() as state_blk, out_dfb.reserve() as o_blk:
            o_blk.store(state_blk)   # STUB — will become state restore

        # Step 2: Load input command (stub: pass through to output).
        # TODO (Task 10): set zm.input_command from input_blk bytes.
        with input_dfb.wait() as input_blk, out_dfb.reserve() as o_blk:
            o_blk.store(input_blk)   # STUB — will become command parse

        # Step 3: Copy first game chunk to output — the primary stub payload.
        # This verifies that the DFB pipeline round-trips data correctly and
        # that header bytes (version, PC, abbrev table) survive the transfer.
        # TODO (Task 10): replace with ZMachineV3.interpret(N) call that
        # produces real game text instead of raw game bytes.
        with game_dfb.wait() as game_blk, out_dfb.reserve() as o_blk:
            o_blk.store(game_blk)    # STUB — will become opcode dispatch

    # ------------------------------------------------------------------
    # DM reader thread
    # ------------------------------------------------------------------
    @ttl.datamovement()
    def dm_reader() -> None:
        """
        Transfer input tensors from DRAM into L1 dataflow buffers.

        Load order must match what compute() consumes:
            1. state  — one CHUNK_SIZE-element block
            2. input  — one CHUNK_SIZE-element block
            3. game   — one CHUNK_SIZE-element block (first chunk only in stub)

        ttl.copy() initiates a NoC transfer; tx.wait() stalls until the
        transfer completes.  The 'with dfb.reserve() as blk:' context manager
        calls blk.push() on exit, signalling compute that the block is ready.

        Full implementation (Task 10):
            Load state[:STATE_SIZE] and input[:INPUT_SIZE] in their entirety
            (each fits in one DFB block at STATE_SIZE/INPUT_SIZE elements).
            Loop over all N_GAME_CHUNKS=170 game chunks:
                for i in range(N_GAME_CHUNKS):
                    with game_dfb.reserve() as g_blk:
                        tx = ttl.copy(game[i*CHUNK_SIZE:(i+1)*CHUNK_SIZE], g_blk)
                        tx.wait()
        """
        # 1. Interpreter state (first CHUNK_SIZE bytes for stub; full STATE_SIZE
        #    in Task 10 using a larger DFB block shape).
        with state_dfb.reserve() as s_blk:
            tx = ttl.copy(state[:CHUNK_SIZE], s_blk)
            tx.wait()

        # 2. Input command string.
        with input_dfb.reserve() as i_blk:
            tx = ttl.copy(input_buf[:CHUNK_SIZE], i_blk)
            tx.wait()

        # 3. Game data — first chunk only (stub).
        # TODO (Task 10): wrap in for i in range(N_GAME_CHUNKS): ... loop.
        with game_dfb.reserve() as g_blk:
            tx = ttl.copy(game[:CHUNK_SIZE], g_blk)
            tx.wait()

    # ------------------------------------------------------------------
    # DM writer thread
    # ------------------------------------------------------------------
    @ttl.datamovement()
    def dm_writer() -> None:
        """
        Transfer output data from L1 dataflow buffers back to DRAM.

        Drains the three output blocks that compute() produces (stub):
            Block 0 — state bytes (scratch; overwrites output[0:CHUNK_SIZE])
            Block 1 — input bytes (scratch; overwrites output[0:CHUNK_SIZE])
            Block 2 — game header bytes (real payload; final value in DRAM)

        Each block must be consumed via copy(o_blk, tensor_slice) to satisfy
        the DM-thread state machine (WAIT → COPY_SRC transition).

        Full implementation (Task 10):
            Write only the real output text (variable length) plus the updated
            interpreter state snapshot for the next kernel invocation.
            Use output[0:text_length] for text and a separate state buffer.
        """
        # Drain block 0: state scratch (final DRAM value will be overwritten).
        with out_dfb.wait() as o_blk:
            tx = ttl.copy(o_blk, output[:CHUNK_SIZE])
            tx.wait()

        # Drain block 1: input scratch (overwritten again below).
        with out_dfb.wait() as o_blk:
            tx = ttl.copy(o_blk, output[:CHUNK_SIZE])
            tx.wait()

        # Drain block 2: the real payload — first CHUNK_SIZE bytes of game data.
        # After this write, output[0:CHUNK_SIZE] contains game[0:CHUNK_SIZE],
        # which the smoke test uses to verify header fields are correct.
        with out_dfb.wait() as o_blk:
            tx = ttl.copy(o_blk, output[:CHUNK_SIZE])
            tx.wait()


# ---------------------------------------------------------------------------
# Host-side helpers — build simulator tensors from raw bytes
# ---------------------------------------------------------------------------

def _bytes_to_sim_tensor(data: bytes | bytearray, target_size: int) -> Tensor:
    """
    Pad *data* with zeros to *target_size* and return a ROW_MAJOR sim Tensor.

    Uses float32 so that ttnn's bfloat16 DRAM path (used on real hardware)
    can represent every integer 0-255 exactly (7 explicit mantissa bits cover
    the full 0-255 range without rounding error — verified in test_device.py).
    """
    buf = bytearray(data)
    if len(buf) < target_size:
        buf += b"\x00" * (target_size - len(buf))
    else:
        buf = buf[:target_size]
    t = torch.frombuffer(buf, dtype=torch.uint8).clone().float()
    return Tensor(t, layout=ROW_MAJOR_LAYOUT)


def _empty_sim_tensor(size: int) -> Tensor:
    """Return a zero-filled ROW_MAJOR sim Tensor of *size* float32 elements."""
    return Tensor(torch.zeros(size, dtype=torch.float32), layout=ROW_MAJOR_LAYOUT)


def run_kernel_sim(
    game_path: str | Path,
    command: str = "",
    state_bytes: bytes = b"",
) -> bytearray:
    """
    Run zmachine_kernel in the TT-Lang simulator (no hardware required).

    Creates host-side sim Tensors for all four buffers, invokes the kernel,
    then returns the output tensor as a bytearray.

    Args:
        game_path:   Path to the Z-machine game file (e.g. ``game/zork1.z3``).
        command:     ASCII command string for the input buffer.  Pass an empty
                     string for the opening sequence (no command yet).
        state_bytes: Serialised interpreter state from the previous turn.
                     Pass empty bytes (default) for a cold start (fresh game).

    Returns:
        bytearray of CHUNK_SIZE bytes.  In the stub, this contains the first
        CHUNK_SIZE bytes of the game file (game header region), which proves
        the DFB pipeline transfers data correctly.  In the full implementation
        (Task 10) this will contain the text output produced by the game.

    Raises:
        FileNotFoundError: if *game_path* does not exist.
        AssertionError:    if the game file version byte is not 3.
    """
    game_bytes = Path(game_path).read_bytes()
    assert game_bytes[0] == 3, (
        f"Expected Z-machine V3 game file, got version {game_bytes[0]}"
    )

    game_tensor   = _bytes_to_sim_tensor(game_bytes, GAME_SIZE)
    state_tensor  = _bytes_to_sim_tensor(state_bytes, STATE_SIZE)  # zeros on cold start
    input_tensor  = _bytes_to_sim_tensor(
        command.encode("ascii") + b"\x00", CHUNK_SIZE
    )
    # Output tensor must be at least CHUNK_SIZE elements (stub writes one chunk).
    output_tensor = _empty_sim_tensor(max(OUTPUT_SIZE, CHUNK_SIZE))

    # Run the operation in the TT-Lang green-thread simulator.
    zmachine_kernel(game_tensor, state_tensor, input_tensor, output_tensor)

    # Extract the output payload (CHUNK_SIZE bytes for the stub).
    raw = output_tensor._tensor[:CHUNK_SIZE].to(torch.uint8).tolist()
    return bytearray(raw)


def run_kernel_on_device(
    game_bytes: bytes,
    state_bytes: bytes,
    command: str,
    device,   # ttnn.Device (type annotation omitted to avoid importing ttnn at module level)
) -> tuple[bytearray, bytearray]:
    """
    Run zmachine_kernel on real QB2 hardware (future Task 10).

    This function is a *placeholder* that documents the device API surface
    needed when wiring the TT-Lang kernel to actual DRAM tensors.

    Args:
        game_bytes:   Complete Z-machine game file (bytes).
        state_bytes:  Serialised interpreter state from the previous turn
                      (or an empty bytes object for a cold start).
        command:      ASCII command string (null-terminated by this function).
        device:       Open ttnn.Device handle (QB2).

    Returns:
        Tuple of (output_bytes, new_state_bytes):
            output_bytes   — text produced by the game this turn
            new_state_bytes — serialised interpreter state for the next call

    Implementation notes (Task 10):
        import ttnn
        game_t   = ttnn.from_torch(game_float,   dtype=ttnn.bfloat16,
                                    layout=ttnn.ROW_MAJOR_LAYOUT, device=device,
                                    memory_config=ttnn.DRAM_MEMORY_CONFIG)
        state_t  = ttnn.from_torch(state_float,  ...)
        input_t  = ttnn.from_torch(input_float,  ...)
        output_t = ttnn.from_torch(output_zeros, ...)
        zmachine_kernel(game_t, state_t, input_t, output_t)
        result = ttnn.to_torch(output_t)
        ...

    The bfloat16 dtype preserves all integers 0-255 exactly (proven in
    tests/ttlang/test_device.py round-trip test).
    """
    raise NotImplementedError(
        "Device execution not yet implemented.  See Task 10 for the full "
        "hardware integration.  Use run_kernel_sim() for simulator testing."
    )


# ---------------------------------------------------------------------------
# Smoke test — run when executed directly
# ---------------------------------------------------------------------------

def _smoke_test(game_path: str) -> None:
    """
    Verify the TT-Lang kernel scaffolding end-to-end in the simulator.

    Checks:
      1. The kernel completes without raising an exception.
      2. output[0] == 3  (Z-machine version byte survives the DFB pipeline).
      3. output[6:8] == 0x50D5  (initial PC from header bytes 6-7).
      4. output[0x18:0x1A] == 0x01F0  (abbreviation table address from header).
      5. output[0:CHUNK_SIZE] exactly matches game[0:CHUNK_SIZE]  (lossless
         transfer of the entire first chunk through DRAM → DFB → DRAM).
    """
    print("[zork_kernel] Running simulator smoke test...")
    print(f"  game:          {game_path}")
    print(f"  GAME_SIZE:     {GAME_SIZE} bytes")
    print(f"  CHUNK_SIZE:    {CHUNK_SIZE} bytes  ({N_GAME_CHUNKS} chunks)")
    print(f"  OUTPUT_SIZE:   {OUTPUT_SIZE} bytes")
    print(f"  STATE_SIZE:    {STATE_SIZE} bytes")
    print(f"  INPUT_SIZE:    {INPUT_SIZE} bytes")
    print()

    output = run_kernel_sim(game_path)

    # 1. Z-machine version byte
    assert output[0] == 3, f"Version byte: got {output[0]}, expected 3"
    print(f"  [OK] Version byte: {output[0]}")

    # 2. Initial PC (bytes 6-7 big-endian)
    pc = (output[0x06] << 8) | output[0x07]
    assert pc == 0x50D5, f"Initial PC: got 0x{pc:04X}, expected 0x50D5"
    print(f"  [OK] Initial PC: 0x{pc:04X}")

    # 3. Abbreviation table address (bytes 0x18-0x19 big-endian)
    abbrev = (output[0x18] << 8) | output[0x19]
    assert abbrev == 0x01F0, f"Abbrev table: got 0x{abbrev:04X}, expected 0x01F0"
    print(f"  [OK] Abbreviation table: 0x{abbrev:04X}")

    # 4. Full chunk integrity
    game_bytes = Path(game_path).read_bytes()
    for i in range(CHUNK_SIZE):
        assert output[i] == game_bytes[i], (
            f"Byte mismatch at offset 0x{i:04X}: "
            f"got {output[i]}, expected {game_bytes[i]}"
        )
    print(f"  [OK] All {CHUNK_SIZE} bytes match game file exactly.")
    print()
    print("[zork_kernel] Smoke test PASSED.")
    print()
    print("Stage 3 scaffolding is working.  Next: Task 10 — replace the")
    print("stub compute() body with ZMachineV3.interpret(N) and validate")
    print("on QB2 hardware.")


if __name__ == "__main__":
    game = sys.argv[1] if len(sys.argv) > 1 else "game/zork1.z3"
    if not Path(game).exists():
        # Try relative to repo root in case of a different working directory.
        candidate = Path(__file__).parent.parent / game
        if candidate.exists():
            game = str(candidate)
        else:
            print(f"Error: game file not found: {game}", file=sys.stderr)
            sys.exit(1)
    _smoke_test(game)
