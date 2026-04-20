# SPDX-License-Identifier: Apache-2.0
"""
Z-machine V3 interpreter in Python, for use with TT-Lang pyenv and ttlang-sim.

Ported from kernels/zork_interpreter_opt.cpp (working RISC-V C++ kernel).
V3 spec: https://inform-fiction.org/zmachine/standards/z1point1/sect11.html

Architecture:
  - ZMachineV3: pure-Python interpreter, no TT-Lang dependencies
  - zork_ttlang.py: wraps ZMachineV3 in a TT-Lang operation for device execution
"""
from __future__ import annotations
from pathlib import Path
from typing import Optional
import struct


class ZMachineV3:
    """Z-machine Version 3 interpreter.

    Reads the game file as a bytes object. All memory accesses use Python
    bytearray for mutability (the game can write to dynamic memory).
    """

    MAX_MEMORY = 131072  # 128KB absolute maximum for V3
    MAX_OUTPUT = 16384   # 16KB output buffer

    def __init__(self, game_bytes: bytes) -> None:
        assert len(game_bytes) >= 64, "Game file too short to be valid"
        # Z-machine memory: mutable copy
        self.memory = bytearray(game_bytes)
        self._parse_header()
        self._init_state()

    # ------------------------------------------------------------------
    # Header parsing — Z-machine V3 header fields
    # ------------------------------------------------------------------

    def _parse_header(self) -> None:
        m = self.memory
        self.version = m[0x00]
        assert self.version == 3, f"Expected V3 game, got V{self.version}"

        # PC: initial value from header bytes 0x06-0x07
        self.initial_pc = (m[0x06] << 8) | m[0x07]

        # Key tables — offsets confirmed against kernels/zork_interpreter_opt.cpp
        self.abbrev_table    = (m[0x18] << 8) | m[0x19]  # abbreviation table
        self.global_vars_addr = (m[0x0C] << 8) | m[0x0D]  # global variables
        self.object_table    = (m[0x0A] << 8) | m[0x0B]  # object table
        self.dictionary_addr = (m[0x08] << 8) | m[0x09]  # dictionary

    def _init_state(self) -> None:
        # Program counter as integer index into self.memory
        self.pc: int = self.initial_pc

        # Stack: list of 16-bit values (grows upward)
        self.stack: list[int] = []

        # Call frames: each frame stores locals, return PC, store_var
        self.frames: list[dict] = []

        # Output buffer
        self.output: list[str] = []

        # Input buffer (set from outside before calling interpret())
        self.input_command: str = ""
        self.input_pos: int = 0

        # Execution state
        self.running: bool = True
        self.game_over: bool = False
        self.instruction_count: int = 0
        self.waiting_for_input: bool = False

    # ------------------------------------------------------------------
    # Memory access helpers
    # ------------------------------------------------------------------

    def read_byte(self, addr: int) -> int:
        if addr < 0 or addr >= len(self.memory):
            return 0
        return self.memory[addr]

    def read_word(self, addr: int) -> int:
        """Read big-endian 16-bit word."""
        if addr < 0 or addr + 1 >= len(self.memory):
            return 0
        return (self.memory[addr] << 8) | self.memory[addr + 1]

    def write_word(self, addr: int, value: int) -> None:
        """Write big-endian 16-bit word."""
        value &= 0xFFFF
        if 0 <= addr < len(self.memory) - 1:
            self.memory[addr]     = (value >> 8) & 0xFF
            self.memory[addr + 1] = value & 0xFF

    def write_byte(self, addr: int, value: int) -> None:
        value &= 0xFF
        if 0 <= addr < len(self.memory):
            self.memory[addr] = value

    def code_byte(self) -> int:
        """Fetch byte at PC and advance."""
        b = self.memory[self.pc]
        self.pc += 1
        return b

    def code_word(self) -> int:
        """Fetch big-endian word at PC and advance."""
        w = (self.memory[self.pc] << 8) | self.memory[self.pc + 1]
        self.pc += 2
        return w

    # ------------------------------------------------------------------
    # Variable access — Z-machine variable numbering:
    #   0x00        = top of stack (pop/push)
    #   0x01-0x0F  = local variables of current frame
    #   0x10-0xFF  = global variables (table at global_vars_addr)
    # ------------------------------------------------------------------

    def get_var(self, var: int) -> int:
        if var == 0:
            return self.stack.pop() if self.stack else 0
        elif 1 <= var <= 15:
            if self.frames and var - 1 < len(self.frames[-1]["locals"]):
                return self.frames[-1]["locals"][var - 1]
            return 0
        else:
            # Global variable
            addr = self.global_vars_addr + (var - 0x10) * 2
            return self.read_word(addr)

    def set_var(self, var: int, value: int) -> None:
        value &= 0xFFFF
        if var == 0:
            self.stack.append(value)
        elif 1 <= var <= 15:
            if self.frames:
                while len(self.frames[-1]["locals"]) <= var - 1:
                    self.frames[-1]["locals"].append(0)
                self.frames[-1]["locals"][var - 1] = value
        else:
            addr = self.global_vars_addr + (var - 0x10) * 2
            self.write_word(addr, value)
