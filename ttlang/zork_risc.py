# SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
# SPDX-License-Identifier: Apache-2.0
"""
zork_risc.py — Run the Z-machine interpreter on QB2 RISC-V via ttnn.generic_op.

This is the Python counterpart of the proven C++ host (zork_on_blackhole.cpp).
It runs kernels/zork_interpreter_l1.cpp on Tenstorrent QB2 RISC-V cores using
the TT-Lang Python environment (ttnn.generic_op + ttnn.KernelDescriptor).

Architecture:
    Host (x86):   Load zork1.z3, allocate DRAM tensors, run kernel, display output.
    Device (RISC-V): kernels/zork_interpreter_l1.cpp — Z-machine interpreter
                      executing 10 Z-machine instructions per invocation (watchdog-safe).

Batched execution — per-batch device sessions:
    The firmware watchdog on QB2 (build 6745986192171285359) limits each kernel run.
    interpret(10) is the only count confirmed safe even when PRINT fires and triggers
    Z-string decode overhead. interpret(20+) hangs at the batch where PRINT fires.

    A deeper issue: the third ttnn.generic_op() call within a single ttnn.open_device()
    session ALWAYS hangs, regardless of instruction count or state content. Confirmed
    by diag_batch3.py: even batch 3 with fresh state (IC=0) hangs.

    Workaround: open and close the ttnn device for EACH batch. The Z-machine
    state (PC, stack, call frames, dynamic game memory) is serialised to
    host-side bytes between sessions via download_state() / upload_state():

        Batch 1: open_device → fresh state → interpret(10) → download_state → close_device
        Batch 2: open_device → upload_state → interpret(10) → download_state → close_device
        Batch 3: open_device → upload_state → interpret(10) → download_state → close_device
        …

    Each batch is the first (and only) kernel invocation in its device session.
    10 batches × 10 instructions = 100 total — sufficient for the Zork opening text.

Key design decisions vs zork_on_blackhole.cpp:
    1. Flat ROW_MAJOR 1D tensors — the interpreter uses noc_async_read(0, 0, addr+offset)
       with hardcoded DRAM bank (0,0). A flat uint8 1D tensor of N elements has
       page_size = N bytes = buffer_size, putting the entire allocation on one DRAM
       bank — matching the C++ fix (page_size = MAX_GAME_SIZE).
    2. Preprocessor defines via KernelDescriptor.defines — the kernel reads GAME_DRAM_ADDR,
       OUTPUT_DRAM_ADDR, INPUT_DRAM_ADDR, and STATE_DRAM_ADDR as #define constants (not
       runtime args — the old get_arg_val() API caused hangs in MeshWorkload contexts).
    3. Single DM reader kernel, no compute thread, no circular buffers — the interpreter
       does its own DRAM→L1 loads via NoC inside kernel_main().
    4. STATE_DRAM_ADDR enables batched execution: the kernel saves/loads ZMachineState
       (PC + stack + call frames + dynamic game memory) between invocations.

Expected output after 10 batches (100 instructions total):
    ZORK I: The Great Underground Empire
    Infocom interactive fiction - a fantasy story
    Copyright 1981, 1982, 1983 Infocom, Inc. All rights reserved.
    ZORK is a registered trademark of Infocom, Inc.
    Release ...
    West of House
    You are standing in an open field west of a white house...

Usage:
    source ~/code/tt-lang/build/env/activate
    cd /home/ttuser/code/tt-zork1
    python ttlang/zork_risc.py [game/zork1.z3]

    # Optional: provide a command input string as second argument
    python ttlang/zork_risc.py game/zork1.z3 "open mailbox"

    # Run with more batches (default is 10):
    ZORK_BATCHES=15 python ttlang/zork_risc.py game/zork1.z3

This file lives at: ttlang/zork_risc.py
L1-resident kernel: kernels/zork_interpreter_l1.cpp  (derived from zork_interpreter_opt.cpp)
Reference kernel:   kernels/zork_interpreter_opt.cpp  (DO NOT MODIFY — reference code)
Proven C++ host:    zork_on_blackhole.cpp
Smoke-test runner:  ttlang/exported/run_kernel.py
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

# Allow running from any directory — add repo root to sys.path
_REPO_ROOT = Path(__file__).parent.parent
sys.path.insert(0, str(_REPO_ROOT))

import torch
import ttnn

# ---------------------------------------------------------------------------
# Paths and buffer geometry
# ---------------------------------------------------------------------------

# Absolute path to the interpreter kernel — KernelDescriptor requires an absolute path.
#
# We use zork_interpreter_l1.cpp, which is derived from the proven reference kernel
# (zork_interpreter_opt.cpp) with one structural change: the large static arrays
# stack[1024] and frames[64] are declared as pointers instead of arrays, and
# initialised in kernel_main() to point into L1 SRAM addresses (0x26000 / 0x26800).
#
# WHY: The newer TT-Metal firmware (build 6745986192171285359) used by TT-Lang Python
# leaves only ~4.8 KB for kernel thread-local data (.bss + .data), but the original
# kernel needs ~5.8 KB (stack=2048B + frames=2304B + rest=~200B).  Moving those two
# arrays to L1 reduces .bss to <200 bytes — well within the firmware limit.
#
# The reference kernel (zork_interpreter_opt.cpp) is NOT modified.
KERNEL_PATH: str = str(_REPO_ROOT / "kernels" / "zork_interpreter_l1.cpp")

# Actual zork1.z3 file size in bytes
GAME_SIZE: int = 86838

# Padded game buffer size: round up to 32-byte alignment (87040 = 85 × 1024)
# The interpreter kernel's constexpr GAME_SIZE = 87040 matches this value.
GAME_PAD: int = 87040

# Output buffer: 16 KB = 16384 bytes. The interpreter writes text here via L1_OUT.
OUTPUT_SIZE: int = 16 * 1024  # 16384

# Input buffer: 1 KB = 1024 bytes for the user command string (null-terminated).
INPUT_SIZE: int = 1024

# State buffer: holds the ZMachineState struct between kernel invocations.
# struct ZMachineState {
#     uint32_t pc_offset, sp, frame_sp;     // 12 bytes
#     zword    stack[1024];                  // 2048 bytes (1024 × uint16_t)
#     Frame    frames[64];                   // ~2560 bytes (64 × ~40-byte Frame)
#     bool     finished;                     // 1 byte
#     uint32_t out_pos, instruction_count;   // 8 bytes (not actually used for out_pos)
# };
# Total: ~4630 bytes for the struct. After 32-byte alignment, dynamic game memory
# (11282 bytes for Zork 1.z3) is appended at DYN_OFFSET. Total ~16434 bytes.
# We allocate 32 KB to accommodate struct + dynamic memory with room to spare.
STATE_SIZE: int = 32 * 1024  # 32768 bytes

# Default number of batches for run_zork_batched().
# 10 batches × 10 instructions = 100 total — enough for the Zork opening text
# including "West of House" room description. Kernel runs interpret(10) per
# invocation; interpret(20+) hangs at the batch where PRINT fires because
# Z-string decode adds overhead beyond the instruction count that tips the
# QB2 firmware watchdog.
# Override with ZORK_BATCHES environment variable.
DEFAULT_BATCHES: int = 10

# Core grid: one core (0,0) — the interpreter is single-threaded.
_CORE = ttnn.CoreCoord(0, 0)
_CORE_RANGES = ttnn.CoreRangeSet([ttnn.CoreRange(_CORE, _CORE)])


# ---------------------------------------------------------------------------
# Buffer helpers
# ---------------------------------------------------------------------------

def load_game(game_path: str | Path, device: ttnn.Device) -> ttnn.Tensor:
    """
    Load zork1.z3 into a flat 1D uint8 ROW_MAJOR DRAM tensor.

    The tensor has GAME_PAD elements (87040), zero-padded from the actual
    GAME_SIZE (86838) to a 32-byte boundary. Each element is one raw byte of
    the game file, stored as uint8 in DRAM.

    Why flat 1D ROW_MAJOR?
        The interpreter kernel loads game data with noc_async_read(0, 0, addr+offset),
        hardcoding DRAM bank (0,0). A flat uint8 tensor has page_size equal to the
        total buffer size (GAME_PAD bytes), so the entire buffer sits on one DRAM
        bank — matching the C++ fix (page_size = MAX_GAME_SIZE).

    Why uint8 (not bfloat16)?
        The RISC-V kernel reads raw bytes via noc_async_read into L1 and accesses them
        as a uint8/byte array (memory[offset]). If we stored each game byte as a
        bfloat16 float (e.g., byte 0x03 → bfloat16(3.0) = 0x4040 in DRAM), the kernel
        would see 0x40 instead of 0x03 at offset 0 — wrong opcode, wrong Z-machine state.
        uint8 stores each game byte as exactly one byte, so noc_async_read delivers the
        correct raw game data directly.

    Args:
        game_path: Path to zork1.z3 (Z-machine V3 binary).
        device:    Open ttnn.Device to allocate the tensor on.

    Returns:
        ttnn.Tensor on device DRAM, shape (GAME_PAD,), dtype uint8, ROW_MAJOR.
    """
    game_bytes = Path(game_path).read_bytes()
    # Verify Z-machine version byte (header byte 0 must be 3 for V3 games)
    assert game_bytes[0] == 3, f"Expected Z-machine V3, got version {game_bytes[0]}"
    assert len(game_bytes) >= 8, "Game file too small (need at least header bytes)"

    # Zero-pad to GAME_PAD bytes so the tensor fills the full buffer
    padded = bytearray(game_bytes[:GAME_SIZE])
    padded += b"\x00" * (GAME_PAD - len(padded))

    # Keep as uint8 — 1 byte per game byte, no float conversion
    t = torch.frombuffer(bytes(padded), dtype=torch.uint8).clone()
    assert t.shape[0] == GAME_PAD, f"Expected {GAME_PAD} elements, got {t.shape[0]}"

    return ttnn.from_torch(
        t,
        dtype=ttnn.uint8,
        layout=ttnn.ROW_MAJOR_LAYOUT,
        device=device,
        memory_config=ttnn.DRAM_MEMORY_CONFIG,
    )


def make_output(device: ttnn.Device) -> ttnn.Tensor:
    """
    Allocate a zero-filled 16 KB output buffer on device DRAM.

    The interpreter kernel writes game text output to L1_OUT (0x30000) and then
    NoC-writes it back to OUTPUT_DRAM_ADDR. This tensor receives that data.

    Args:
        device: Open ttnn.Device.

    Returns:
        ttnn.Tensor on device DRAM, shape (OUTPUT_SIZE,), dtype bfloat16, ROW_MAJOR.
    """
    t = torch.zeros(OUTPUT_SIZE, dtype=torch.float32)
    return ttnn.from_torch(
        t,
        dtype=ttnn.bfloat16,
        layout=ttnn.ROW_MAJOR_LAYOUT,
        device=device,
        memory_config=ttnn.DRAM_MEMORY_CONFIG,
    )


def make_input(device: ttnn.Device, command: str = "") -> ttnn.Tensor:
    """
    Allocate a 1 KB input buffer on device DRAM and populate it with a command.

    The interpreter's READ opcode (VAR 0x04) reads the command string from
    L1_INPUT (0x34000), which is loaded from INPUT_DRAM_ADDR at kernel start.
    Stored as uint8 so each ASCII character occupies exactly one byte in DRAM,
    matching what noc_async_read delivers to the kernel's input buffer.

    Args:
        device:  Open ttnn.Device.
        command: Zork command string (e.g., "open mailbox"). Empty = no input.

    Returns:
        ttnn.Tensor on device DRAM, shape (INPUT_SIZE,), dtype uint8, ROW_MAJOR.
    """
    buf = bytearray(INPUT_SIZE)  # zero-filled (null-terminated empty string)
    if command:
        # Write command as null-terminated ASCII, truncated to buffer size - 1
        cmd_bytes = command.encode("ascii", errors="replace")[:INPUT_SIZE - 1]
        buf[:len(cmd_bytes)] = cmd_bytes
    t = torch.frombuffer(bytes(buf), dtype=torch.uint8).clone()
    return ttnn.from_torch(
        t,
        dtype=ttnn.uint8,
        layout=ttnn.ROW_MAJOR_LAYOUT,
        device=device,
        memory_config=ttnn.DRAM_MEMORY_CONFIG,
    )


def make_state(device: ttnn.Device) -> ttnn.Tensor:
    """
    Allocate a zero-filled 16 KB state buffer on device DRAM.

    The interpreter kernel's ZMachineState struct (~4.6 KB of actual data) is saved
    here between batches so the Python host can run multiple kernel invocations
    while staying within the firmware watchdog limit.

    A zero-filled state is interpreted by the kernel as "first batch" (the kernel
    checks `state->instruction_count > 0` to decide whether to load or init fresh).
    The Python host should NOT reset this buffer between batches — the kernel
    increments `instruction_count` to signal it has real state to restore.

    WHY uint8 (not bfloat16):
        The kernel saves/loads ZMachineState as raw binary struct bytes via NOC.
        Using uint8 ensures 1 DRAM byte per tensor element — the tensor buffer
        IS the raw byte storage for the struct. Using bfloat16 would not change
        the actual DRAM byte layout (the kernel always writes/reads raw bytes via
        noc_async_write/noc_async_read), but uint8 avoids confusion and matches
        the dtype of the game and input buffers.

        Crucially, uint8 with STATE_SIZE = 16384 gives exactly 16384 bytes of
        DRAM storage — enough for sizeof(ZMachineState) ≈ 4632 bytes.

    Args:
        device: Open ttnn.Device.

    Returns:
        ttnn.Tensor on device DRAM, shape (STATE_SIZE,), dtype uint8, ROW_MAJOR.
        Initially all zeros (instruction_count == 0 → fresh init on first batch).
    """
    t = torch.zeros(STATE_SIZE, dtype=torch.uint8)
    return ttnn.from_torch(
        t,
        dtype=ttnn.uint8,
        layout=ttnn.ROW_MAJOR_LAYOUT,
        device=device,
        memory_config=ttnn.DRAM_MEMORY_CONFIG,
    )


def read_output(output_t: ttnn.Tensor) -> str:
    """
    Extract the text string from the output tensor.

    The RISC-V interpreter kernel writes raw ASCII bytes directly to DRAM via NoC.
    The tensor is declared as bfloat16, so ttnn.to_torch converts the 16-bit DRAM
    values to float32 by interpreting them as bfloat16 floats — which would corrupt
    raw ASCII bytes.

    Correct approach: convert back to bfloat16, then view the underlying memory as
    uint8 bytes. This recovers the raw bytes the kernel wrote without any float
    reinterpretation.

    Example:
        Kernel writes 'H'=0x48, 'E'=0x45 → DRAM bytes [0x48, 0x45]
        Old (wrong): interpret as bfloat16(0x4548) → float(3200.0) → 3200 & 0xFF = 128
        New (correct): view as uint8 → bytes [0x48, 0x45] → 'H', 'E'  ✓

    Args:
        output_t: Output tensor returned by make_output() after kernel execution.

    Returns:
        Decoded ASCII string containing the game's output text.
    """
    t_bf16 = ttnn.to_torch(output_t).to(torch.bfloat16)
    raw = bytes(t_bf16.view(torch.uint8).numpy())
    end = raw.find(b"\x00")
    if end >= 0:
        raw = raw[:end]
    return raw.decode("ascii", errors="replace")


# ---------------------------------------------------------------------------
# State serialisation helpers (for per-batch device sessions)
# ---------------------------------------------------------------------------

def download_state(state_t: ttnn.Tensor) -> bytes:
    """
    Read the state tensor from device DRAM to host memory as raw bytes.

    Called after each kernel invocation to capture the Z-machine interpreter
    state (PC, stack, call frames, dynamic memory) before closing the device.
    The bytes are passed to upload_state() at the start of the next batch.

    The state tensor is uint8, so ttnn.to_torch() returns a uint8 tensor
    with no float conversion — the raw bytes are preserved exactly as written
    by the kernel's save_state() function.

    Args:
        state_t: State tensor (uint8, shape (STATE_SIZE,)) from the last kernel run.

    Returns:
        Raw bytes of the ZMachineState struct + dynamic memory snapshot.
    """
    t_host = ttnn.to_torch(state_t)          # uint8 tensor on CPU
    return bytes(t_host.numpy().tobytes())   # raw bytes, no reinterpretation


def upload_state(device: ttnn.Device, state_bytes: bytes) -> ttnn.Tensor:
    """
    Create a pre-populated state tensor from host-side bytes and upload to device DRAM.

    Called at the start of each batch (after batch 1) to restore the Z-machine
    state saved by the previous batch's kernel invocation. The kernel checks
    state->instruction_count > 0 to decide whether to resume or init fresh.

    Args:
        device:      Open ttnn.Device.
        state_bytes: Bytes returned by download_state() from the previous batch.

    Returns:
        ttnn.Tensor on device DRAM, shape (STATE_SIZE,), dtype uint8, ROW_MAJOR,
        pre-populated with the saved interpreter state.
    """
    # Pad or trim to STATE_SIZE to ensure exact tensor shape
    buf = bytearray(STATE_SIZE)
    n = min(len(state_bytes), STATE_SIZE)
    buf[:n] = state_bytes[:n]
    t = torch.frombuffer(bytes(buf), dtype=torch.uint8).clone()
    return ttnn.from_torch(
        t,
        dtype=ttnn.uint8,
        layout=ttnn.ROW_MAJOR_LAYOUT,
        device=device,
        memory_config=ttnn.DRAM_MEMORY_CONFIG,
    )


# ---------------------------------------------------------------------------
# Core runner
# ---------------------------------------------------------------------------

def run_interpreter(
    game_t: ttnn.Tensor,
    output_t: ttnn.Tensor,
    input_t: ttnn.Tensor,
    device: ttnn.Device,
    state_t: ttnn.Tensor | None = None,
) -> None:
    """
    Execute kernels/zork_interpreter_l1.cpp on QB2 RISC-V via ttnn.generic_op.

    One kernel invocation = one batch = 40 Z-machine instructions. For the full
    game opening (100+ instructions), call this multiple times with the same state_t.

    The kernel sequence:
        1. Reads game data from DRAM (GAME_DRAM_ADDR) into L1 at 0x10000
        2. Reads input from DRAM (INPUT_DRAM_ADDR) into L1 at 0x34000
        3. If STATE_DRAM_ADDR defined: loads saved ZMachineState from DRAM into L1 at 0x50000
           - If state.instruction_count == 0: fresh init from Z-machine header
           - If state.instruction_count > 0:  resume from saved PC, stack, call frames
        4. Runs interpret(20) — 20 Z-machine instructions (watchdog-safe on QB2)
        5. If STATE_DRAM_ADDR defined: saves updated state back to DRAM
        6. NoC-writes output text (starting at out_pos=0 each batch) back to OUTPUT_DRAM_ADDR

    Compile-time defines (KernelDescriptor.defines):
        GAME_DRAM_ADDR   — Physical DRAM address of game tensor buffer
        OUTPUT_DRAM_ADDR — Physical DRAM address of output tensor buffer
        INPUT_DRAM_ADDR  — Physical DRAM address of input tensor buffer
        STATE_DRAM_ADDR  — (optional) Physical DRAM address of state tensor buffer;
                           presence enables batched/resumable execution mode

    The kernel uses plain noc_async_read(get_noc_addr(0, 0, addr+offset), L1_dst, size)
    for data loading — it does NOT use TensorAccessors or CBs. This requires flat,
    single-bank DRAM buffers. Game, input, and state tensors are uint8 (1 byte/element,
    so page_size = N bytes = entire buffer, placing all on one DRAM bank). Output tensor
    is bfloat16 (page_size = 2×N bytes = entire buffer, also single-bank). All four
    tensors are on DRAM bank 0 (NOC coord 0,0), which the kernel hardcodes.

    Args:
        game_t:   Game data tensor (loaded by load_game()).
        output_t: Output buffer tensor (from make_output()); written in-place each batch.
        input_t:  Input command tensor (from make_input()).
        device:   Open ttnn.Device (inferred from game_t if needed).
        state_t:  State tensor (from make_state(), uint8 dtype) for batched execution.
                  When provided, the kernel persists ZMachineState (PC + stack + call
                  frames) between invocations. Pass the SAME tensor object to all
                  batches — the kernel overwrites it with updated state after each run.
    """
    # Collect DRAM buffer addresses — these become preprocessor #defines
    game_addr   = game_t.buffer_address()
    output_addr = output_t.buffer_address()
    input_addr  = input_t.buffer_address()

    # Build the defines list: Sequence[tuple[str, str]] (name, value pairs)
    # The kernel expects hex string literals like "0x493e40" — Python hex() works.
    defines: list[tuple[str, str]] = [
        ("GAME_DRAM_ADDR",   hex(game_addr)),
        ("OUTPUT_DRAM_ADDR", hex(output_addr)),
        ("INPUT_DRAM_ADDR",  hex(input_addr)),
    ]
    if state_t is not None:
        state_addr = state_t.buffer_address()
        defines.append(("STATE_DRAM_ADDR", hex(state_addr)))

    # Build KernelDescriptor for the RISC-V data-movement kernel.
    #
    # In the Python ttnn API (QB2/Blackhole), config descriptor selection:
    #   ReaderConfigDescriptor  → NCRISC processor — CONFIRMED WORKING
    #   WriterConfigDescriptor  → BRISC processor  — silently fails (0.002s, no output)
    #   DataMovementConfigDescriptor(RISCV_0, NOC0) → BRISC — also silently fails
    #
    # Empirically tested 2026-04-21: BRISC kernels return in 0.002s with zero output —
    # they appear to execute silently without doing work (device-side issue unknown).
    # NCRISC kernels run for ~0.4s per batch and produce correct output.
    #
    # The C++ host (zork_on_blackhole.cpp) used DataMovementProcessor::RISCV_0 (BRISC),
    # but it used the direct CreateKernel API on a single device, not MeshDevice/generic_op.
    # The Python generic_op path requires NCRISC for correct execution.
    #
    # No compile_time_args (no CB indices — the interpreter doesn't use CBs).
    # No common_runtime_args (addresses are passed via defines, not runtime args;
    # the old get_arg_val() API caused hangs in MeshWorkload contexts per CLAUDE.md).
    kernel_desc = ttnn.KernelDescriptor(
        kernel_source=KERNEL_PATH,
        source_type=ttnn.KernelDescriptor.SourceType.FILE_PATH,
        core_ranges=_CORE_RANGES,
        compile_time_args=[],           # no CB indices needed
        named_compile_time_args=[],     # none
        defines=defines,                # GAME/OUTPUT/INPUT/STATE_DRAM_ADDR
        common_runtime_args=[],         # addresses passed via defines (not runtime args)
        config=ttnn.ReaderConfigDescriptor(),  # NCRISC — empirically confirmed working
    )

    # Build ProgramDescriptor: one kernel, no CBs, no semaphores.
    # The interpreter manages its own L1 layout (0x10000–0x60000).
    program = ttnn.ProgramDescriptor(
        kernels=[kernel_desc],
        cbs=[],          # no circular buffers — interpreter uses raw L1 addresses
        semaphores=[],   # no inter-thread synchronization needed
    )

    # Execute on device. Tensors must be on the same device.
    # generic_op dispatches the program to the Tenstorrent hardware.
    #
    # Tensor ordering: generic_op treats the LAST tensor as the "output" tensor
    # (see generic_op_device_operation.cpp: io_tensors.back()). We always put
    # output_t last so the framework treats it as the output — even when state_t
    # is present. The kernel accesses all buffers via hardcoded DRAM #defines, so
    # tensor ordering in this list does NOT affect kernel behavior.
    if state_t is not None:
        all_tensors = [game_t, input_t, state_t, output_t]  # output_t last = "the output"
    else:
        all_tensors = [game_t, input_t, output_t]

    ttnn.generic_op(all_tensors, program)


# ---------------------------------------------------------------------------
# Top-level entry point — batched multi-invocation execution
# ---------------------------------------------------------------------------

def run_zork(
    game_path: str | Path,
    command: str = "",
    verbose: bool = True,
    num_batches: int | None = None,
) -> str:
    """
    Run Zork I on QB2 RISC-V using per-batch device sessions and return the output text.

    The firmware watchdog limits each kernel invocation to ~10 Z-machine instructions
    (interpret(20+) hangs when PRINT fires). Additionally, the third call to
    ttnn.generic_op() within a single ttnn.open_device() session always hangs,
    regardless of instruction count or state content.

    This function works around both limits by opening and closing the ttnn device
    for EACH batch. The Z-machine state (PC, stack, call frames, dynamic game memory)
    is serialised to host-side bytes between sessions via download_state() / upload_state().
    Each batch is therefore the first (and only) kernel invocation in its device session
    — confirmed reliable even when PRINT fires.

    10 batches × 10 instructions = 100 total instructions produces the full Zork
    opening sequence including "West of House" and the first room description.

    Args:
        game_path:   Path to zork1.z3 (Z-machine V3 binary).
        command:     Optional input command (e.g., "open mailbox"). Empty = no input.
        verbose:     Print progress messages to stdout.
        num_batches: Number of 10-instruction batches to run (default: DEFAULT_BATCHES=10).
                     Override via ZORK_BATCHES env var.

    Returns:
        Accumulated game output text across all batches (non-empty batches only).
    """
    game_path = Path(game_path)
    if not game_path.exists():
        raise FileNotFoundError(f"Game file not found: {game_path}")

    # Determine batch count: argument > env var > default
    if num_batches is None:
        env_batches = os.environ.get("ZORK_BATCHES", "")
        num_batches = int(env_batches) if env_batches.isdigit() else DEFAULT_BATCHES

    if verbose:
        print(f"[zork_risc] Game:     {game_path} ({game_path.stat().st_size} bytes)")
        print(f"[zork_risc] Kernel:   {KERNEL_PATH}")
        print(f"[zork_risc] Command:  {command!r}")
        print(f"[zork_risc] Batches:  {num_batches} × 10 instructions = {num_batches*10} total")
        print(f"[zork_risc] Strategy: per-batch device sessions (workaround for 3rd-invocation hang)")
        print()

    # saved_state: host-side bytes of ZMachineState from previous batch.
    # None on first batch → kernel does fresh init (instruction_count == 0).
    saved_state: bytes | None = None
    all_text: list[str] = []
    seen_output = False  # True once we have seen at least one non-empty batch

    for batch in range(num_batches):
        if verbose:
            print(f"[zork_risc] Batch {batch + 1}/{num_batches}: opening device...", flush=True)

        device = ttnn.open_device(device_id=0)
        try:
            game_t   = load_game(game_path, device)
            output_t = make_output(device)
            input_t  = make_input(device, command)

            # First batch: fresh zeroed state (instruction_count == 0 → fresh init).
            # Subsequent batches: restore state from previous batch via upload_state().
            if saved_state is None:
                state_t = make_state(device)
            else:
                state_t = upload_state(device, saved_state)

            if verbose:
                print(f"  game:   {game_t.buffer_address():#010x}")
                print(f"  output: {output_t.buffer_address():#010x}")
                print(f"  input:  {input_t.buffer_address():#010x}")
                print(f"  state:  {state_t.buffer_address():#010x}  ({'fresh' if saved_state is None else 'restored'})")

            run_interpreter(game_t, output_t, input_t, device, state_t=state_t)

            batch_text = read_output(output_t)
            all_text.append(batch_text)

            # Save state to host before closing device
            saved_state = download_state(state_t)

        finally:
            ttnn.close_device(device)

        if batch_text.strip():
            seen_output = True

        if verbose:
            n_chars = len(batch_text.strip())
            print(f"  → {n_chars} chars of output", flush=True)
            if batch_text.strip():
                preview = batch_text.strip()[:200]
                print(f"  → preview: {preview!r}", flush=True)

        # Only stop early once we HAVE seen game output and it then stops.
        # Do NOT stop in the silent warm-up batches before the first PRINT fires.
        if seen_output and not batch_text.strip():
            if verbose:
                print("  → no output after previous content — game finished or stalled")
            break

    return "\n".join(t for t in all_text if t.strip())


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    # Usage: python ttlang/zork_risc.py [game/zork1.z3] ["command string"]
    game_arg = sys.argv[1] if len(sys.argv) > 1 else "game/zork1.z3"
    cmd_arg  = sys.argv[2] if len(sys.argv) > 2 else ""

    # Resolve game path relative to repo root if needed
    game_path = Path(game_arg)
    if not game_path.exists():
        candidate = _REPO_ROOT / game_arg
        if candidate.exists():
            game_path = candidate
        else:
            print(f"Error: game file not found: {game_arg}", file=sys.stderr)
            sys.exit(1)

    print("╔══════════════════════════════════════════════════════════╗")
    print("║  ZORK I on Tenstorrent QB2 RISC-V via Python / ttnn    ║")
    print("║  1977 Gaming Technology on 2026 AI Accelerator Silicon  ║")
    print("╚══════════════════════════════════════════════════════════╝")
    print()

    text = run_zork(game_path, command=cmd_arg, verbose=True)

    print("╔════════════════════════════════════════════════════╗")
    print("║  ZORK OUTPUT FROM QB2 RISC-V CORE                 ║")
    print("╠════════════════════════════════════════════════════╣")
    print(text)
    print("╚════════════════════════════════════════════════════╝")

    # Validate the output contains the expected Zork title
    if "ZORK" in text:
        print()
        print("[OK] 'ZORK' found in output — Z-machine interpreter executed successfully!")
        sys.exit(0)
    elif text.strip():
        print()
        print(f"[WARN] Output present but 'ZORK' not found (got {len(text)} chars).")
        print("       May need more instructions or data loading issue.")
        sys.exit(0)
    else:
        print()
        print("[FAIL] No output generated — kernel may have failed silently.")
        sys.exit(1)
