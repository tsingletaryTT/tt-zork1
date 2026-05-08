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
import random


class ZMachineV3:
    """Z-machine Version 3 interpreter.

    Reads the game file as a bytes object. All memory accesses use Python
    bytearray for mutability (the game can write to dynamic memory).
    """

    MAX_MEMORY = 131072  # 128KB absolute maximum for V3
    MAX_OUTPUT = 16384   # 16KB output buffer

    def __init__(self, game_bytes: bytes) -> None:
        assert len(game_bytes) >= 64, "Game file too short to be valid"
        # Keep a pristine copy of the original ROM for RESTART opcode.
        # self.memory may be mutated by the game; _original_bytes never is.
        self._original_bytes: bytes = bytes(game_bytes)
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
        if self.version not in (3, 5):
            raise ValueError(f"Unsupported Z-machine version {self.version} (supported: 3, 5)")

        # PC: initial value from header bytes 0x06-0x07
        self.initial_pc = (m[0x06] << 8) | m[0x07]

        # Key tables — offsets confirmed against kernels/zork_interpreter_opt.cpp
        self.abbrev_table    = (m[0x18] << 8) | m[0x19]  # abbreviation table
        self.global_vars_addr = (m[0x0C] << 8) | m[0x0D]  # global variables
        self.object_table    = (m[0x0A] << 8) | m[0x0B]  # object table
        self.dictionary_addr = (m[0x08] << 8) | m[0x09]  # dictionary

        # Packed-address multipliers differ by version (Z-machine spec §1.2.3).
        # V3: packed = addr × 2.  V5: packed = addr × 4 + 8 × header_offset.
        if self.version <= 3:
            self._packed_mult = 2
            self._routine_offset = 0
            self._string_offset  = 0
        else:
            self._packed_mult = 4
            self._routine_offset = ((m[0x28] << 8) | m[0x29]) * 8
            self._string_offset  = ((m[0x2A] << 8) | m[0x2B]) * 8

        # Object table layout differs by version (Z-machine spec §12).
        # V3: 31 default property words (62 bytes), 9-byte entries,
        #     single-byte parent/sibling/child, 31 attributes, 31 max props.
        # V5: 63 default property words (126 bytes), 14-byte entries,
        #     word-sized parent/sibling/child, 48 attributes, 63 max props.
        if self.version <= 3:
            self._obj_defaults_size = 62   # 31 × 2
            self._obj_entry_size    = 9
            self._obj_max_attrs     = 31
            self._obj_max_props     = 31
            self._obj_prop_ptr_off  = 7    # offset within entry to prop-table pointer word
        else:
            self._obj_defaults_size = 126  # 63 × 2
            self._obj_entry_size    = 14
            self._obj_max_attrs     = 47
            self._obj_max_props     = 63
            self._obj_prop_ptr_off  = 12

    def _init_state(self) -> None:
        # Set interpreter capability headers (Z-machine spec §11.1).
        # The interpreter must fill these before the game starts executing.
        # We write directly to self.memory (dynamic memory), not _original_bytes.
        if self.version >= 4:
            self.memory[0x20] = 1       # interpreter number: 1 = DEC-20 (neutral)
            self.memory[0x21] = ord('1') # interpreter version: '1'
            self.memory[0x22] = 255     # screen height: 255 = infinite (no pagination)
            self.memory[0x23] = 80      # screen width: 80 columns (standard)
            # Standard revision: 1.2 (spec §11.1.7)
            self.memory[0x32] = 1
            self.memory[0x33] = 2
        if self.version >= 5:
            # Flags 1 (byte 0x01) — set capabilities we support
            # Bit 2 = bold, bit 3 = italic, bit 4 = fixed-space
            self.memory[0x01] |= (1 << 4)  # fixed-space font available
            self.memory[0x24] = 0          # screen width in px (unknown → 0)
            self.memory[0x25] = 0
            self.memory[0x26] = 1          # font width in px (1 = character-cell)
            self.memory[0x27] = 1          # font height in px

        # Program counter as integer index into self.memory
        self.pc: int = self.initial_pc

        # Stack: list of 16-bit values (grows upward)
        self.stack: list[int] = []

        # Call frames: each frame stores locals, return PC, store_var
        self.frames: list[dict] = []

        # Output buffer
        self.output: list[str] = []
        # Current output window (0 = lower/main, 1 = upper/status bar).
        # SET_WINDOW switches this; we suppress output when not in window 0.
        self.current_window: int = 0

        # Input buffer (set from outside before calling interpret())
        self.input_command: str = ""
        self.input_pos: int = 0

        # Execution state
        self.running: bool = True
        self.game_over: bool = False
        self.instruction_count: int = 0
        self.waiting_for_input: bool = False
        # PC saved at the start of each instruction dispatch; used by READ to
        # restore the PC when no input is available, so READ re-executes on resume.
        self._instr_pc: int = self.initial_pc

    # ------------------------------------------------------------------
    # Memory access helpers
    # ------------------------------------------------------------------

    def read_byte(self, addr: int) -> int:
        if addr < 0 or addr >= len(self.memory):
            return 0
        return self.memory[addr]

    def read_word(self, addr: int) -> int:
        """Read big-endian 16-bit word."""
        if addr < 0 or addr + 2 > len(self.memory):
            return 0
        return (self.memory[addr] << 8) | self.memory[addr + 1]

    def write_word(self, addr: int, value: int) -> None:
        """Write big-endian 16-bit word."""
        value &= 0xFFFF
        if 0 <= addr and addr + 2 <= len(self.memory):
            self.memory[addr]     = (value >> 8) & 0xFF
            self.memory[addr + 1] = value & 0xFF

    def write_byte(self, addr: int, value: int) -> None:
        value &= 0xFF
        if 0 <= addr < len(self.memory):
            self.memory[addr] = value

    def code_byte(self) -> int:
        """Fetch byte at PC and advance. Stops execution on PC overflow."""
        if self.pc >= len(self.memory):
            self.running = False
            return 0
        b = self.memory[self.pc]
        self.pc += 1
        return b

    def code_word(self) -> int:
        """Fetch big-endian word at PC and advance. Stops execution on PC overflow."""
        if self.pc + 2 > len(self.memory):
            self.running = False
            return 0
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
            self.stack.append(value)  # spec §4.6: writing to var 0 pushes onto stack
        elif 1 <= var <= 15:
            if self.frames:
                while len(self.frames[-1]["locals"]) <= var - 1:
                    self.frames[-1]["locals"].append(0)
                self.frames[-1]["locals"][var - 1] = value
        else:
            addr = self.global_vars_addr + (var - 0x10) * 2
            self.write_word(addr, value)

    # ------------------------------------------------------------------
    # Z-string decoding — Z-machine spec section 3
    # ------------------------------------------------------------------
    # Z-strings are sequences of 16-bit words. Each word packs three 5-bit
    # character codes (bits 14-10, 9-5, 4-0). Bit 15 set = end of string.
    #
    # 5-bit code meanings:
    #   0        → space
    #   1-3      → abbreviation (next code gives index into abbrev table)
    #   4        → shift to alphabet A1 (uppercase) for next char only
    #   5        → shift to alphabet A2 (punctuation) for next char only
    #   6-31     → character in current alphabet (A0=lower, A1=upper, A2=punct)
    #   In A2: code 6 = ZSCII escape (next TWO 5-bit values form 10-bit char)
    #          code 7 = newline
    # ------------------------------------------------------------------

    # Alphabet tables — indexed by raw 5-bit code (6..31 → position 0..25).
    # Code 0=space, 1-5=special (handled above), 6-31=these chars.
    _ALPHA0 = "abcdefghijklmnopqrstuvwxyz"   # A0: lowercase
    _ALPHA1 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"   # A1: uppercase
    _ALPHA2 = "0123456789.,!?_#'\"/\\-:()"   # A2: punctuation (codes 8-31; 6=escape, 7=newline)

    def _get_char(self, alph: int, idx: int) -> str:
        """Map a raw 5-bit code (idx, range 6-31) in alphabet alph to a character.

        Codes 0-5 are special and handled by the main decoder loop.
        For A0 and A1: idx 6→'a'/'A', …, idx 31→'z'/'Z'.
        For A2: idx 6 = ZSCII escape (10-bit), idx 7 = newline,
                idx 8→'0', …, idx 31→last punct char.
        """
        if idx < 6:
            # Should not be called for special codes, but be safe
            return "?"
        pos = idx - 6  # 0-based position within the alphabet
        if alph == 0:
            return self._ALPHA0[pos] if pos < len(self._ALPHA0) else "?"
        elif alph == 1:
            return self._ALPHA1[pos] if pos < len(self._ALPHA1) else "?"
        else:
            # A2: codes 6 and 7 are handled in the main loop (escape / newline)
            pos2 = idx - 8  # code 8 → index 0 in punct string
            return self._ALPHA2[pos2] if 0 <= pos2 < len(self._ALPHA2) else "?"

    def _decode_abbrev(self, code: int, index: int, depth: int) -> str:
        """Expand abbreviation code 1-3 with follow-on index into the abbrev table.

        The abbreviation table (at self.abbrev_table) holds word addresses.
        Entry number = (code - 1) * 32 + index.
        Each entry is a word address; multiply by 2 to get the byte address
        of the Z-string to decode.

        depth guards against infinite recursion (spec disallows nested abbrevs,
        but malformed data could loop).
        """
        if depth >= 3 or code < 1 or code > 3:
            return ""
        abbrev_idx = (code - 1) * 32 + index
        entry_addr = self.abbrev_table + abbrev_idx * 2
        word_addr = self.read_word(entry_addr)
        byte_addr = word_addr * 2
        return self._decode_zstring_raw(byte_addr, 30, depth + 1)

    def _decode_zstring_raw(self, addr: int, max_words: int, depth: int) -> str:
        """Low-level Z-string decoder (Z-machine spec §3).

        Reads up to max_words 16-bit words from self.memory starting at addr.
        Returns the decoded plain-text string.

        depth is the abbreviation recursion depth (0 = top-level call).
        The spec forbids abbreviations inside abbreviations; depth >= 3 is
        used as a safety limit to prevent infinite recursion on malformed data.

        State machine variables:
          shift   — current alphabet (0=A0 lowercase, 1=A1 uppercase, 2=A2 punct)
          abbrev  — pending abbreviation trigger (0=none, 1/2/3=pending)
          zscii_stage — 0=normal, 1=collecting high 5 bits of 10-bit ZSCII,
                        2=collecting low 5 bits of 10-bit ZSCII
          zscii_hi — high 5 bits collected during stage 1
        """
        if addr < 0 or addr >= len(self.memory) or depth >= 3:
            return ""

        chars: list[str] = []
        shift = 0        # current alphabet: 0=A0, 1=A1, 2=A2
        abbrev: int = 0  # pending abbreviation code (0 = none)
        zscii_stage = 0  # 0=normal, 1=need high bits, 2=need low bits
        zscii_hi = 0     # accumulated high 5 bits for 10-bit ZSCII escape

        for _ in range(max_words):
            if addr + 1 >= len(self.memory):
                break
            word = (self.memory[addr] << 8) | self.memory[addr + 1]
            addr += 2

            # Each 16-bit word contains three 5-bit codes, left to right
            for shift_pos in (10, 5, 0):
                c = (word >> shift_pos) & 0x1F

                # Priority 1: completing a 10-bit ZSCII escape sequence.
                # A2 code 6 triggers a two-step escape: the next two 5-bit codes
                # form a 10-bit ZSCII character number (high bits first).
                if zscii_stage == 1:
                    zscii_hi = c           # store high 5 bits
                    zscii_stage = 2
                    continue
                if zscii_stage == 2:
                    zscii_char = (zscii_hi << 5) | c   # combine into 10-bit ZSCII
                    # ZSCII overlaps ASCII for the printable range 32-126
                    chars.append(chr(zscii_char) if 32 <= zscii_char <= 126 else "?")
                    zscii_stage = 0
                    shift = 0
                    continue

                # Priority 2: completing an abbreviation lookup.
                # Codes 1-3 in the main stream signal an abbreviation; the very
                # next code is the index into the abbreviation table.
                if abbrev:
                    chars.append(self._decode_abbrev(abbrev, c, depth))
                    abbrev = 0
                    shift = 0
                    continue

                # Priority 3: normal code dispatch.
                if c == 0:
                    # Code 0 = space character
                    chars.append(" ")
                    shift = 0
                elif 1 <= c <= 3:
                    # Abbreviation trigger — consume next code as index
                    abbrev = c
                    shift = 0
                elif c == 4:
                    # Single-character shift to A1 (uppercase)
                    shift = 1
                elif c == 5:
                    # Single-character shift to A2 (punctuation)
                    shift = 2
                elif shift == 2 and c == 6:
                    # A2 escape: start collecting a 10-bit ZSCII code
                    zscii_stage = 1
                    zscii_hi = 0
                    shift = 0
                elif shift == 2 and c == 7:
                    # A2 code 7 = newline
                    chars.append("\n")
                    shift = 0
                else:
                    # Regular character — look up in current alphabet
                    chars.append(self._get_char(shift, c))
                    shift = 0  # shifts are single-use in V3 (not persistent locks)

            if word & 0x8000:  # bit 15 set = end-of-string marker
                break

        return "".join(chars)

    def decode_zstring(self, addr: int) -> str:
        """Decode the Z-string starting at byte address addr.

        This is the public entry point. Uses a generous 300-word limit
        (600 bytes of encoded Z-string = ~900 Z-chars before abbreviation
        expansion) so that long strings like the Zork I leaflet decode fully.
        Starts at recursion depth 0. Returns plain text with abbreviations expanded.
        """
        return self._decode_zstring_raw(addr, 300, 0)

    def get_object_name(self, obj_num: int) -> str:
        """Return the text name of Z-machine object obj_num (1-indexed).

        The object entry layout and sizes depend on the Z-machine version
        (see _parse_header for _obj_defaults_size, _obj_entry_size, _obj_prop_ptr_off).

        The property table for each object starts with:
          byte 0:   length of object name in Z-string words
          bytes 1+: the Z-string name

        Returns empty string if obj_num is out of range or has no name.
        """
        if obj_num < 1 or obj_num > (255 if self.version <= 3 else 65535):
            return ""

        obj_base = self._obj_addr(obj_num)
        prop_ptr = self.read_word(obj_base + self._obj_prop_ptr_off)
        if prop_ptr == 0:
            return ""

        # Property table header: byte 0 = name length in Z-string words
        name_len_words = self.memory[prop_ptr]
        if name_len_words == 0:
            return ""  # object has no name

        # The Z-string name follows immediately at prop_ptr + 1
        return self.decode_zstring(prop_ptr + 1)

    # ------------------------------------------------------------------
    # Operand loading — Z-machine V3 instruction formats
    # ------------------------------------------------------------------

    def _load_operand(self, op_type: int) -> int:
        """Load one operand. Types: 0=large const, 1=small const, 2=variable, 3=omitted.

        Operand type encoding (Z-machine spec §4.2):
          0b00 (0) → large constant (2 bytes, big-endian)
          0b01 (1) → small constant (1 byte)
          0b10 (2) → variable — read variable number from PC, then dereference
          0b11 (3) → omitted (caller must not call for omitted operands)

        Note: bit-test approach (& 2, & 1) matches Frotz's implementation and
        handles any stray high bits safely — see frotz_os.c process.c lines 197-216.
        """
        if op_type & 2:      # bit 1 set → variable
            return self.get_var(self.code_byte())
        elif op_type & 1:    # bit 0 set → small constant (1 byte)
            return self.code_byte()
        else:                # 0b00 → large constant (2 bytes)
            return self.code_word()

    def _load_var_operands(self) -> list[int]:
        """Load operands from the types byte used in VAR-form instructions.

        In VAR form, a single byte immediately after the opcode byte encodes
        up to four operand types (2 bits each, MSB first):
          bits 7-6 → type of operand 0
          bits 5-4 → type of operand 1
          bits 3-2 → type of operand 2
          bits 1-0 → type of operand 3
        Type 3 (0b11) means omitted; once we see it, all remaining are also omitted.
        """
        types_byte = self.code_byte()
        operands: list[int] = []
        for shift in (6, 4, 2, 0):
            t = (types_byte >> shift) & 3
            if t == 3:  # omitted — stop here
                break
            operands.append(self._load_operand(t))
        return operands

    def _load_var_operands_double(self) -> list[int]:
        """Load up to 8 operands using two consecutive type bytes (CALL_VS2 / CALL_VN2).

        Per Z-machine spec §4.3.3: for double-VAR instructions, both type bytes are
        read consecutively BEFORE any operand data. types1 encodes operand types 0-3,
        types2 encodes types 4-7. All operand data follows after both type bytes.
        Stops immediately on encountering type 3 (omitted) in either byte.
        """
        types1 = self.code_byte()
        types2 = self.code_byte()
        operands: list[int] = []
        for types_byte in (types1, types2):
            for shift in (6, 4, 2, 0):
                t = (types_byte >> shift) & 3
                if t == 3:  # omitted — stop all remaining loading
                    return operands
                operands.append(self._load_operand(t))
        return operands

    def _branch(self, condition: bool) -> None:
        """Handle a branch instruction. Reads branch data from PC.

        Branch encoding (Z-machine spec §4.7):
          byte 0: bit 7 = branch-if-true flag (1=branch on true, 0=branch on false)
                  bit 6 = short-offset flag (1=6-bit offset in this byte, 0=14-bit)
                  bits 5-0 = offset (if bit 6 set) or high 6 bits of 14-bit offset

          If bit 6 is clear, byte 1 provides the low 8 bits of the 14-bit offset.

          Special offset values (after full decode):
            0 → RFALSE (return false from current routine)
            1 → RTRUE  (return true from current routine)
            n → PC += n - 2  (relative jump; -2 because offset is from after branch bytes)

          Sign extension for long form: the 14-bit offset is treated as a signed
          integer (bit 13 is the sign bit), giving a range of -8192..+8191.
        """
        branch_byte = self.code_byte()
        branch_if_true = bool(branch_byte & 0x80)
        if branch_byte & 0x40:
            # Short form: 6-bit offset in this byte
            offset = branch_byte & 0x3F
        else:
            # Long form: 14-bit offset across two bytes
            offset = ((branch_byte & 0x3F) << 8) | self.code_byte()
            # Sign-extend: if bit 13 set, offset is negative
            if offset & 0x2000:
                offset -= 0x4000

        if condition == branch_if_true:
            if offset == 0:
                self._do_ret(0)    # RFALSE
            elif offset == 1:
                self._do_ret(1)    # RTRUE
            else:
                self.pc += offset - 2

    def _store_result(self, value: int) -> None:
        """Read a result variable number from PC and store value there.

        Many Z-machine instructions produce a result (e.g. ADD, CALL).
        The variable to store into is encoded as the next byte after the
        instruction's operands (and before any branch byte).
        Variable 0 = top of stack (push), 1-15 = locals, 16+ = globals.
        """
        var = self.code_byte()
        self.set_var(var, value)

    # ------------------------------------------------------------------
    # Output helpers
    # ------------------------------------------------------------------

    def _print_char(self, c: str) -> None:
        """Append a single character to the output buffer (suppressed in non-main windows)."""
        if self.current_window == 0:
            self.output.append(c)

    def _print_str(self, s: str) -> None:
        """Append a string to the output buffer (suppressed in non-main windows)."""
        if self.current_window == 0:
            self.output.append(s)

    def flush_output(self) -> str:
        """Return accumulated output as a single string and clear the buffer.

        Useful for interactive sessions: call this after interpret() to get
        everything the game printed since the last flush.
        """
        text = "".join(self.output)
        self.output.clear()
        return text

    # ------------------------------------------------------------------
    # Call/return mechanism
    # ------------------------------------------------------------------

    def _do_call(self, packed_addr: int, args: list[int], store_var: int) -> None:
        """Call a Z-machine routine.

        packed_addr == 0 is a special case: by spec, calling address 0 is a
        no-op that stores 0 (false) into store_var without creating a frame.

        V3 uses word-packed addresses: multiply by 2 to get the byte address
        of the routine. The routine starts with one byte giving the number of
        local variables (0-15), then that many 2-byte default values, followed
        by the instruction stream.

        Any arguments supplied by the caller override the defaults, left-to-right.
        Extra arguments beyond num_locals are ignored; missing arguments leave
        the corresponding locals at their default values.

        The call frame saved onto self.frames records everything needed to restore
        state on return: the PC to return to, the local variables, and the
        variable number where the return value should be stored.
        """
        if packed_addr == 0:
            if store_var != -1:
                self.set_var(store_var, 0)
            return

        body_addr = packed_addr * self._packed_mult + self._routine_offset
        num_locals = self.memory[body_addr]
        body_addr += 1  # skip num-locals byte

        # V1-4: routine header contains default values for each local (one word each).
        # V5+: no default values in header; all locals initialize to 0.
        if self.version <= 4:
            locals_: list[int] = []
            for i in range(num_locals):
                locals_.append(self.read_word(body_addr))
                body_addr += 2
        else:
            locals_: list[int] = [0] * num_locals  # type: ignore[assignment]

        # Override with supplied arguments
        for i, arg in enumerate(args):
            if i < len(locals_):
                locals_[i] = arg

        # Push call frame; body_addr now points to first instruction of the routine
        self.frames.append({
            "ret_pc": self.pc,
            "locals": locals_,
            "store_var": store_var,
        })
        self.pc = body_addr

    def _do_ret(self, value: int) -> None:
        """Return value from the current routine.

        Pops the innermost call frame, restores the saved PC, and stores
        the return value into the frame's designated store variable.
        If no frames remain (top-level return), halt execution.
        """
        if not self.frames:
            self.running = False
            return
        frame = self.frames.pop()
        self.pc = frame["ret_pc"]
        if frame["store_var"] != -1:
            self.set_var(frame["store_var"], value)

    # ------------------------------------------------------------------
    # interpret() — main execution loop
    # ------------------------------------------------------------------

    def interpret(self, max_instructions: int = 100) -> None:
        """Execute up to max_instructions Z-machine instructions.

        Stops early if self.running is set to False (e.g. by a QUIT or RESTART
        opcode, a PC overflow, or a top-level return from the main routine).
        Also stops if self.waiting_for_input becomes True (set by READ opcode).
        After returning, accumulated output is in self.output.

        Resume semantics: if input_command is set when interpret() is called
        while waiting_for_input is True, the flag is cleared so the READ opcode
        at the current PC will be re-executed to consume the provided input.
        """
        # If the interpreter was paused waiting for input and input has now been
        # provided, clear the flag. The PC was already backed up to the READ
        # instruction by _do_read, so re-executing will process the new input.
        if self.waiting_for_input and self.input_command:
            self.waiting_for_input = False

        for _ in range(max_instructions):
            if not self.running or self.waiting_for_input:
                break
            self._instr_pc = self.pc  # save PC for READ back-up on no-input
            self._execute_one()
            self.instruction_count += 1

    def _execute_one(self) -> None:
        """Fetch and execute one Z-machine instruction.

        Z-machine instruction encoding (spec §4):
          Two high bits of opcode byte select the instruction form:
            0b11xxxxxx (0xC0-0xFF) → VAR form  (opcode in low 5 bits)
            0b10xxxxxx (0x80-0xBF) → Short form (opcode in low 4 bits)
            0b00/01... (0x00-0x7F) → Long form  (opcode in low 5 bits, always 2OP)

          Short form can be 1OP or 0OP depending on bits 4-5:
            0b11 (3) in bits 5-4 → 0OP (zero operands)
            other              → 1OP (one operand; bits 5-4 are its type)

          Long form is always 2OP:
            bit 6 → type of first operand  (0=small const, 1=variable)
            bit 5 → type of second operand (0=small const, 1=variable)
            Large constants are NOT valid in long form per the spec.

        Special case: opcode byte 0xBE introduces EXT form (V5+). V3 should
        never encounter it, but we skip the following byte as a safety measure.
        """
        opcode_byte = self.code_byte()

        # EXT form (V5+): 0xBE <ext-opcode> <operand-types-byte> [operands] [store] [branch]
        # We implement the handful of EXT opcodes that matter for game startup;
        # the rest are consumed and skipped so the PC stays correct.
        if opcode_byte == 0xBE:
            ext_op = self.code_byte()
            types_byte = self.code_byte()
            # Decode up to 4 operands from the types byte (same encoding as VAR form).
            operands = []
            for shift in (6, 4, 2, 0):
                t = (types_byte >> shift) & 0x03
                if t == 3:
                    break
                elif t == 0:
                    operands.append(self.code_word())
                elif t == 1:
                    operands.append(self.code_byte())
                else:
                    v = self.code_byte()
                    operands.append(self.get_var(v))
            # EXT 0x09 SAVE_UNDO / EXT 0x0A RESTORE_UNDO: store −1 (unsupported).
            if ext_op in (0x09, 0x0A):
                store_var = self.code_byte()
                self.set_var(store_var, 0xFFFF)  # −1 = feature unavailable
            # EXT 0x04 SET_FONT: store 0 (previous font, signals unsupported).
            elif ext_op == 0x04:
                store_var = self.code_byte()
                self.set_var(store_var, 0)
            # All other EXT opcodes: skip optional store byte if opcode implies one.
            # Opcodes 0x00-0x03 (SAVE,RESTORE,LOG_SHIFT,ART_SHIFT) store a result.
            elif ext_op in (0x00, 0x01, 0x02, 0x03):
                store_var = self.code_byte()
                self.set_var(store_var, 0)
            return

        form = opcode_byte >> 6          # 0b11=VAR, 0b10=short, else long

        if form == 0b11:
            # VAR form: operand-types byte follows opcode byte (encodes up to 4 operands).
            # Bit 5 of the opcode byte distinguishes two sub-forms (Z-machine spec §4.3.3):
            #
            #   0xC0-0xDF  (bit 5 == 0):  2OP opcode encoded in variable form.
            #     The low 5 bits are a 2OP opcode number (same table as long-form 2OP).
            #     This allows 2OP instructions to have up to 4 operands and large
            #     constants.  Only the first two operands are meaningful for dispatch.
            #     Examples: 0xC9 = AND (2OP 0x09), 0xC1 = JE (2OP 0x01).
            #
            #   0xE0-0xFF  (bit 5 == 1):  Proper VAR-specific opcode.
            #     The low 5 bits are a VAR opcode number.
            #     Examples: 0xE0 = CALL (VAR 0x00), 0xE6 = PRINT_NUM (VAR 0x06),
            #               0xE9 = PULL (VAR 0x09).
            #
            # Frotz implements this via a unified var_opcodes[] table (process.c):
            #   indices 0-31  → 2OP opcodes (used for 0xC0-0xDF)
            #   indices 32-63 → VAR-specific opcodes (used for 0xE0-0xFF)
            # We mirror that split here.
            opcode = opcode_byte & 0x1F
            # CALL_VS2 (0x0C) and CALL_VN2 (0x1A) use TWO operand-type bytes.
            # Per spec §4.3.3, both type bytes come BEFORE all operand data.
            # _load_var_operands_double reads both type bytes then all operands.
            if (opcode_byte & 0x20) and opcode in (0x0C, 0x1A):
                operands = self._load_var_operands_double()
            else:
                operands = self._load_var_operands()
            if opcode_byte & 0x20:
                # Bit 5 set → proper VAR opcode (0xE0-0xFF)
                self._dispatch_var(opcode, operands)
            else:
                # Bit 5 clear → 2OP in VAR form (0xC0-0xDF)
                # Only the first two operands matter for 2OP dispatch.
                opcode = opcode_byte & 0x1F
                a = operands[0] if len(operands) > 0 else 0
                b = operands[1] if len(operands) > 1 else 0
                self._dispatch_2op(opcode, a, b)

        elif form == 0b10:
            # Short form: 1OP or 0OP
            op_type = (opcode_byte >> 4) & 3
            if op_type == 3:
                # 0OP — opcode in low 4 bits, no operands
                opcode = opcode_byte & 0x0F
                self._dispatch_0op(opcode)
            else:
                # 1OP — bits 4-5 are operand type, opcode in low 4 bits
                opcode = opcode_byte & 0x0F
                operand = self._load_operand(op_type)
                self._dispatch_1op(opcode, operand)

        else:
            # Long form: always 2OP; opcode in low 5 bits
            # Bit 6 selects type for operand A, bit 5 for operand B.
            # Mapping: bit set → variable (type 2), bit clear → small const (type 1).
            # Large constants (type 0) do NOT appear in long form.
            opcode = opcode_byte & 0x1F
            type0 = 2 if (opcode_byte & 0x40) else 1
            type1 = 2 if (opcode_byte & 0x20) else 1
            a = self._load_operand(type0)
            b = self._load_operand(type1)
            self._dispatch_2op(opcode, a, b)

    # ------------------------------------------------------------------
    # Stub dispatchers — filled in by Tasks 5 and 6
    # ------------------------------------------------------------------
    # These are intentionally empty stubs. All instruction decoding and
    # operand loading above is complete; the actual opcode implementations
    # (PRINT, CALL, JE, ADD, etc.) are added in the next tasks.
    # Having stubs here allows the interpret() loop to run without crashing,
    # even though no output is produced yet.

    def _dispatch_2op(self, opcode: int, a: int, b: int) -> None:
        """Dispatch a 2OP instruction.

        Covers all V3 2OP opcodes: comparisons/branches (JE, JL, JG),
        variable inc/dec with branch (DEC_CHK, INC_CHK), object tree
        queries (JIN, TEST_ATTR), bitwise logic (OR, AND, TEST),
        attribute mutation (SET_ATTR, CLEAR_ATTR), object manipulation
        (STORE, INSERT_OBJ), memory loads (LOADW, LOADB), property
        access (GET_PROP, GET_PROP_ADDR, GET_NEXT_PROP), and arithmetic
        (ADD, SUB, MUL, DIV, MOD).

        Unknown opcodes are silently skipped (safe for forward-compat).
        """
        if opcode == 0x01:   # JE — jump if equal
            self._branch(a == b)

        elif opcode == 0x02: # JL — jump if less (signed)
            sa = a if a < 0x8000 else a - 0x10000
            sb = b if b < 0x8000 else b - 0x10000
            self._branch(sa < sb)

        elif opcode == 0x03: # JG — jump if greater (signed)
            sa = a if a < 0x8000 else a - 0x10000
            sb = b if b < 0x8000 else b - 0x10000
            self._branch(sa > sb)

        elif opcode == 0x04: # DEC_CHK — decrement var a, branch if result < b (signed)
            val = (self.get_var(a) - 1) & 0xFFFF
            self.set_var(a, val)
            signed_val = val if val < 0x8000 else val - 0x10000
            signed_b   = b   if b   < 0x8000 else b   - 0x10000
            self._branch(signed_val < signed_b)

        elif opcode == 0x05: # INC_CHK — increment var a, branch if result > b (signed)
            val = (self.get_var(a) + 1) & 0xFFFF
            self.set_var(a, val)
            signed_val = val if val < 0x8000 else val - 0x10000
            signed_b   = b   if b   < 0x8000 else b   - 0x10000
            self._branch(signed_val > signed_b)

        elif opcode == 0x06: # JIN — jump if object a is child of b
            parent = self._get_obj_parent(a)
            self._branch(parent == b)

        elif opcode == 0x07: # TEST — bit test: branch if (a & b) == b
            self._branch((a & b) == b)

        elif opcode == 0x08: # OR — bitwise or
            self._store_result(a | b)

        elif opcode == 0x09: # AND — bitwise and
            self._store_result(a & b)

        elif opcode == 0x0A: # TEST_ATTR — branch if object a has attribute b
            self._branch(self._test_attr(a, b))

        elif opcode == 0x0B: # SET_ATTR — set attribute b on object a
            self._set_attr(a, b)

        elif opcode == 0x0C: # CLEAR_ATTR — clear attribute b on object a
            self._clear_attr(a, b)

        elif opcode == 0x0D: # STORE — store value b into variable a
            self.set_var(a, b)

        elif opcode == 0x0E: # INSERT_OBJ — reparent a to b
            self._insert_obj(a, b)

        elif opcode == 0x0F: # LOADW — load word: result = mem[a + b*2]
            addr = (a + b * 2) & 0xFFFF
            self._store_result(self.read_word(addr))

        elif opcode == 0x10: # LOADB — load byte: result = mem[a + b]
            addr = (a + b) & 0xFFFF
            self._store_result(self.read_byte(addr))

        elif opcode == 0x11: # GET_PROP — get property b of object a
            self._store_result(self._get_prop(a, b))

        elif opcode == 0x12: # GET_PROP_ADDR — get address of property b of object a
            self._store_result(self._get_prop_addr(a, b))

        elif opcode == 0x13: # GET_NEXT_PROP — get next property after b for object a
            self._store_result(self._get_next_prop(a, b))

        elif opcode == 0x14: # ADD
            self._store_result((a + b) & 0xFFFF)

        elif opcode == 0x15: # SUB
            self._store_result((a - b) & 0xFFFF)

        elif opcode == 0x16: # MUL
            self._store_result((a * b) & 0xFFFF)

        elif opcode == 0x17: # DIV — signed division (truncate toward zero)
            if b == 0:
                self._store_result(0)
            else:
                sa = a if a < 0x8000 else a - 0x10000
                sb = b if b < 0x8000 else b - 0x10000
                self._store_result(int(sa / sb) & 0xFFFF)

        elif opcode == 0x18: # MOD — signed modulo
            if b == 0:
                self._store_result(0)
            else:
                sa = a if a < 0x8000 else a - 0x10000
                sb = b if b < 0x8000 else b - 0x10000
                self._store_result((sa - int(sa / sb) * sb) & 0xFFFF)

        elif opcode == 0x19: # CALL_2S (V4+) — call routine a with arg b, store result
            store_var = self.code_byte()
            self._do_call(a, [b], store_var)

        elif opcode == 0x1A: # CALL_2N (V5+) — call routine a with arg b, discard result
            self._do_call(a, [b], store_var=-1)

        elif opcode == 0x1B: # SET_COLOUR (V5+) — no-op in line-based mode
            pass

        elif opcode == 0x1C: # THROW (V5+) — unwind stack to frame n
            # Pop frames until we reach frame number b, then return value a.
            target_depth = b if b < 0x8000 else b - 0x10000
            while len(self.frames) > target_depth:
                self.frames.pop()
            if self.frames:
                frame = self.frames.pop()
                self.pc = frame["ret_pc"]
                if frame["store_var"] != -1:
                    self.set_var(frame["store_var"], a)
            else:
                self.running = False

        # Unknown 2OP opcodes are silently skipped

    def _dispatch_1op(self, opcode: int, a: int) -> None:
        """Dispatch a 1OP instruction.

        Covers all V3 1OP opcodes: conditional branch (JZ), object tree
        navigation (GET_SIBLING, GET_CHILD, GET_PARENT), property length
        query (GET_PROP_LEN), variable mutation (INC, DEC), string printing
        (PRINT_ADDR, PRINT_OBJ, PRINT_PADDR), object removal (REMOVE_OBJ),
        routine return (RET), unconditional jump (JUMP), variable copy
        (LOAD), and bitwise NOT (NOT).

        CALL_1S reads an additional store_var byte from PC before calling.
        JUMP encodes offset in the operand (no separate branch byte).
        """
        if opcode == 0x00:   # JZ — jump if zero
            self._branch(a == 0)

        elif opcode == 0x01: # GET_SIBLING — get sibling of object a; branch if non-zero
            sib = self._get_obj_sibling(a)
            self._store_result(sib)
            self._branch(sib != 0)

        elif opcode == 0x02: # GET_CHILD — get first child of object a; branch if non-zero
            child = self._get_obj_child(a)
            self._store_result(child)
            self._branch(child != 0)

        elif opcode == 0x03: # GET_PARENT — get parent of object a
            self._store_result(self._get_obj_parent(a))

        elif opcode == 0x04: # GET_PROP_LEN — get length of property at data address a
            # Operand a is the address of property DATA (first byte after header).
            # V3: 1-byte header; plen = (bits 7-5) + 1.
            # V5: the byte at a-1 is the last header byte; if its bit 7 is set it's
            #     the second byte of a 2-byte header and plen = bits 5-0 (0 → 64).
            #     Otherwise it's a 1-byte header and plen = (bit 6 ? 2 : 1).
            if a == 0:
                self._store_result(0)
            else:
                sb = self.memory[a - 1]
                if self.version <= 3:
                    prop_len = (sb >> 5) + 1
                elif sb & 0x80:
                    prop_len = sb & 0x3F
                    if prop_len == 0:
                        prop_len = 64
                else:
                    prop_len = ((sb >> 6) & 1) + 1
                self._store_result(prop_len)

        elif opcode == 0x05: # INC — increment variable a
            self.set_var(a, (self.get_var(a) + 1) & 0xFFFF)

        elif opcode == 0x06: # DEC — decrement variable a
            self.set_var(a, (self.get_var(a) - 1) & 0xFFFF)

        elif opcode == 0x07: # PRINT_ADDR — print Z-string at byte address a
            self._print_str(self.decode_zstring(a))

        elif opcode == 0x08: # CALL_1S — call routine at packed addr a; store result
            # The store variable byte follows in the instruction stream
            store_var = self.code_byte()
            self._do_call(a, [], store_var)

        elif opcode == 0x09: # REMOVE_OBJ — detach object a from its parent
            self._remove_obj(a)

        elif opcode == 0x0A: # PRINT_OBJ — print name of object a
            self._print_str(self.get_object_name(a))

        elif opcode == 0x0B: # RET — return value a from current routine
            self._do_ret(a)

        elif opcode == 0x0C: # JUMP — unconditional branch via signed operand offset
            # The operand IS the signed offset (not a separate branch byte).
            # PC has already advanced past the operand bytes when we get here,
            # so: final PC = PC + offset - 2  (the spec's "-2" corrects for that).
            offset = a if a < 0x8000 else a - 0x10000
            self.pc += offset - 2

        elif opcode == 0x0D: # PRINT_PADDR — print Z-string at packed address a
            self._print_str(self.decode_zstring(a * self._packed_mult + self._string_offset))

        elif opcode == 0x0E: # LOAD — copy variable a's value to result variable
            self._store_result(self.get_var(a))

        elif opcode == 0x0F:
            # V3/V4: NOT — bitwise NOT, store result.
            # V5+: CALL_1N — call routine at packed addr a, discard result (no store byte).
            if self.version >= 5:
                self._do_call(a, [], store_var=-1)
            else:
                self._store_result((~a) & 0xFFFF)

    def _dispatch_0op(self, opcode: int) -> None:
        """Dispatch a 0OP instruction (Z-machine spec §14, 0OP opcodes).

        0OP instructions have no operands. The opcode is in the low 4 bits of
        the opcode byte when in short form with bits 5-4 == 0b11.

        Opcodes implemented:
          0x00 RTRUE     — return 1 from current routine
          0x01 RFALSE    — return 0 from current routine
          0x02 PRINT     — print inline Z-string (encoded in code stream)
          0x03 PRINT_RET — print inline Z-string + newline, then RTRUE
          0x04 NOP       — no operation
          0x05 SAVE      — V3 branch form; always report failure (not implemented)
          0x06 RESTORE   — V3 branch form; always report failure (not implemented)
          0x07 RESTART   — restore original ROM and reinitialize interpreter state
          0x08 RET_POPPED — return TOS value
          0x09 POP       — discard TOS
          0x0A QUIT      — end game execution
          0x0B NEW_LINE  — print newline character
          0x0C SHOW_STATUS — no-op (status bar not rendered in line-based simulator)
          0x0D VERIFY    — V3 branch; always reports checksum OK (true)
        """
        if opcode == 0x00:   # RTRUE — return true (1) from current routine
            self._do_ret(1)

        elif opcode == 0x01: # RFALSE — return false (0) from current routine
            self._do_ret(0)

        elif opcode == 0x02: # PRINT — inline Z-string immediately follows in code stream
            # Record start of Z-string, then advance PC past it by scanning for
            # the end-of-string marker: high bit (bit 15) set in a 16-bit word.
            start = self.pc
            while self.pc + 1 < len(self.memory):
                word = (self.memory[self.pc] << 8) | self.memory[self.pc + 1]
                self.pc += 2
                if word & 0x8000:  # bit 15 = end-of-string marker
                    break
            self._print_str(self.decode_zstring(start))

        elif opcode == 0x03: # PRINT_RET — print inline Z-string, newline, then RTRUE
            start = self.pc
            while self.pc + 1 < len(self.memory):
                word = (self.memory[self.pc] << 8) | self.memory[self.pc + 1]
                self.pc += 2
                if word & 0x8000:
                    break
            self._print_str(self.decode_zstring(start))
            self._print_char("\n")
            self._do_ret(1)

        elif opcode == 0x04: # NOP — no operation
            pass

        elif opcode == 0x05: # SAVE (V3 branch form) — always report failure in simulator
            # Real SAVE writes game state to disk; we don't implement that.
            # Branching on False tells the game the save failed (it usually handles gracefully).
            self._branch(False)

        elif opcode == 0x06: # RESTORE (V3 branch form) — always report failure in simulator
            self._branch(False)

        elif opcode == 0x07: # RESTART — restore original ROM bytes and reinitialize
            # Must reload from _original_bytes (not from self.memory, which may be
            # mutated by the game). Then re-parse the header and reset all state.
            self.memory = bytearray(self._original_bytes)
            self._parse_header()
            self._init_state()

        elif opcode == 0x08: # RET_POPPED — return with top-of-stack value
            self._do_ret(self.stack.pop() if self.stack else 0)

        elif opcode == 0x09: # POP — discard top of stack
            if self.stack:
                self.stack.pop()

        elif opcode == 0x0A: # QUIT — terminate game execution
            self.running = False
            self.game_over = True

        elif opcode == 0x0B: # NEW_LINE — output a newline character
            self._print_char("\n")

        elif opcode == 0x0C: # SHOW_STATUS — no-op (status bar not rendered in line-based simulator)
            pass

        elif opcode == 0x0D: # VERIFY (V3 branch) — checksum verification; always pass
            # We don't verify the checksum; just tell the game everything is fine.
            self._branch(True)

    def _dispatch_var(self, opcode: int, operands: list[int]) -> None:
        """Dispatch a VAR-form instruction (Z-machine spec §14, VAR opcodes).

        VAR-form instructions have 0-4 operands encoded via a types byte that
        follows the opcode byte (see _load_var_operands). Operand types are
        read before dispatch; this method receives the already-loaded values.

        Opcodes implemented:
          0x00 CALL        — call routine, store result
          0x01 STOREW      — mem[a + b*2] = c
          0x02 STOREB      — mem[a + b] = c & 0xFF
          0x03 PUT_PROP    — set object property
          0x04 READ/SREAD  — read player input into buffers
          0x05 PRINT_CHAR  — print single ZSCII character
          0x06 PRINT_NUM   — print signed integer
          0x07 RANDOM      — pseudo-random number generator
          0x08 PUSH        — push value onto stack
          0x09 PULL        — pop from stack into variable
          0x0A SPLIT_WINDOW — no-op (line-based simulator has no split)
          0x0B SET_WINDOW  — no-op
          0x13 OUTPUT_STREAM — no-op
          0x14 INPUT_STREAM  — no-op
          0x15 SOUND_EFFECT  — no-op
        """
        def arg(n: int, default: int = 0) -> int:
            """Return operand n, or default if not present."""
            return operands[n] if n < len(operands) else default

        if opcode == 0x00:   # CALL — call routine; store result byte follows operands
            if not operands:
                return
            packed_addr = operands[0]
            call_args = operands[1:]
            # The store variable is encoded as the next byte AFTER all operands,
            # NOT in the operand-types byte. Always read it from the code stream.
            store_var = self.code_byte()
            self._do_call(packed_addr, call_args, store_var)

        elif opcode == 0x01: # STOREW — store 16-bit word: mem[a + b*2] = c
            self.write_word((arg(0) + arg(1) * 2) & 0xFFFF, arg(2))

        elif opcode == 0x02: # STOREB — store byte: mem[a + b] = c & 0xFF
            self.write_byte((arg(0) + arg(1)) & 0xFFFF, arg(2) & 0xFF)

        elif opcode == 0x03: # PUT_PROP — write value to object property
            self._put_prop(arg(0), arg(1), arg(2))

        elif opcode == 0x04: # READ (SREAD/AREAD) — read player input
            # _do_read handles waiting_for_input itself: if input_command is empty
            # it sets the flag and returns; caller sets input_command and calls
            # interpret() again to resume. Do NOT set the flag here.
            self._do_read(arg(0), arg(1))
            # V5+ AREAD is a store instruction: after operands, a store byte encodes
            # the variable to receive the terminating character (13 = Enter key).
            # Only consume the store byte when input was actually processed
            # (waiting_for_input=False means _do_read did not back up the PC).
            if self.version >= 5 and not self.waiting_for_input:
                self._store_result(13)  # 13 = ZSCII newline = Enter key

        elif opcode == 0x05: # PRINT_CHAR — print single ZSCII character
            ch = arg(0)
            if 32 <= ch <= 126:
                self._print_char(chr(ch))
            elif ch == 13:       # ZSCII carriage-return = newline
                self._print_char("\n")
            elif ch == 0:        # NUL = nothing
                pass
            # Other ZSCII codes silently discarded (not in printable ASCII range)

        elif opcode == 0x06: # PRINT_NUM — print signed 16-bit integer
            n = arg(0)
            if n >= 0x8000:   # sign-extend from 16-bit two's complement
                n -= 0x10000
            self._print_str(str(n))

        elif opcode == 0x07: # RANDOM — pseudo-random number generator
            limit = arg(0)
            if limit > 0:
                # Return random integer in 1..limit, store result
                self._store_result(random.randint(1, limit))
            else:
                # limit <= 0 → reseed RNG (limit 0 = system seed, negative = seeded)
                random.seed(abs(limit) if limit != 0 else None)
                self._store_result(0)

        elif opcode == 0x08: # PUSH — push value onto interpreter stack
            self.stack.append(arg(0))

        elif opcode == 0x09: # PULL — pop from stack, store into variable
            # Note: do NOT call get_var(arg(0)) — that would pop the stack again.
            # Instead, pop explicitly and then store with set_var.
            val = self.stack.pop() if self.stack else 0
            self.set_var(arg(0), val)

        elif opcode == 0x0A: # SPLIT_WINDOW — no-op (single-stream simulator)
            pass

        elif opcode == 0x0B: # SET_WINDOW — track current window to suppress status-bar output
            # Window 0 = lower/main window, window 1 = upper/status bar.
            # Output printed while in window 1 is status-bar content — suppress it.
            self.current_window = arg(0) & 0xFFFF

        elif opcode == 0x0C: # CALL_VS2 (V5+) — CALL_VS with up to 8 args (two type bytes)
            # The second operand-types byte was already consumed by _load_var_operands
            # for double-type VAR; operands already holds all args including routine addr.
            if not operands:
                return
            packed_addr = operands[0]
            call_args = operands[1:]
            store_var = self.code_byte()
            self._do_call(packed_addr, call_args, store_var)

        elif opcode == 0x0D: # ERASE_WINDOW (V4+) — no-op
            pass

        elif opcode == 0x0E: # ERASE_LINE (V4+) — no-op
            pass

        elif opcode == 0x0F: # SET_CURSOR (V4+) — no-op
            pass

        elif opcode == 0x10: # GET_CURSOR (V4+) — store (0,0); no cursor tracking
            # operand is the array address to store row,col into; we leave it as-is
            pass

        elif opcode == 0x11: # SET_TEXT_STYLE (V4+) — no-op (plain text only)
            pass

        elif opcode == 0x12: # BUFFER_MODE (V4+) — no-op
            pass

        elif opcode == 0x13: # OUTPUT_STREAM — no-op (stream management)
            pass

        elif opcode == 0x14: # INPUT_STREAM — no-op
            pass

        elif opcode == 0x15: # SOUND_EFFECT — no-op (no audio in simulator)
            pass

        elif opcode == 0x16: # READ_CHAR (V4+) — read a single character
            # For simplicity treat like a READ that consumes one char of input.
            store_var = self.code_byte()
            if self.input_command:
                ch = ord(self.input_command[0])
                self.input_command = self.input_command[1:]
            else:
                ch = 13  # carriage return = Enter
            self.set_var(store_var, ch)

        elif opcode == 0x17: # SCAN_TABLE (V5+) — search word/byte table
            # Scan table at addr for value x in len entries; store found addr or 0.
            x, addr, length = arg(0), arg(1), arg(2)
            form = arg(3, 0x82)   # default: word entries (0x82 = words, 0x80 = bytes)
            entry_size = form & 0x7F
            word_form = bool(form & 0x80)
            found_addr = 0
            for i in range(length):
                ea = addr + i * entry_size
                v = self.read_word(ea) if word_form else self.read_byte(ea)
                if v == x:
                    found_addr = ea
                    break
            store_var = self.code_byte()
            self.set_var(store_var, found_addr)
            self._branch(found_addr != 0)

        elif opcode == 0x18: # NOT (V5+, moved from 1OP) — bitwise NOT, store result
            store_var = self.code_byte()
            self.set_var(store_var, (~arg(0)) & 0xFFFF)

        elif opcode == 0x19: # CALL_VN (V5+) — call routine, discard result
            if not operands:
                return
            packed_addr = operands[0]
            call_args = operands[1:]
            self._do_call(packed_addr, call_args, store_var=-1)

        elif opcode == 0x1A: # CALL_VN2 (V5+) — CALL_VN with up to 8 args
            if not operands:
                return
            packed_addr = operands[0]
            call_args = operands[1:]
            self._do_call(packed_addr, call_args, store_var=-1)

        elif opcode == 0x1B: # TOKENISE (V5+) — tokenise text buffer
            pass  # Leave the parse buffer as-is; game will still read input

        elif opcode == 0x1C: # ENCODE_TEXT (V5+) — encode text to Z-chars
            pass

        elif opcode == 0x1D: # COPY_TABLE (V5+) — copy memory region
            src, dst, size = arg(0), arg(1), arg(2)
            if dst == 0:
                # zero out src table
                for i in range(abs(size)):
                    self.write_byte(src + i, 0)
            else:
                n = abs(size)
                data = [self.read_byte(src + i) for i in range(n)]
                for i, b in enumerate(data):
                    self.write_byte(dst + i, b)

        elif opcode == 0x1E: # PRINT_TABLE (V5+) — print text table to screen
            pass

        elif opcode == 0x1F: # CHECK_ARG_COUNT (V5+) — branch if arg n was supplied
            # We don't track arg count precisely; always branch true for safety.
            self._branch(True)

        # Unknown VAR opcodes are silently skipped (safe for forward-compat)

    # ------------------------------------------------------------------
    # Object operations — layout is version-specific (see _parse_header):
    #   V3: 62-byte defaults, 9-byte entries, 1-byte parent/sib/child, 31 attrs
    #   V5: 126-byte defaults, 14-byte entries, 2-byte parent/sib/child, 48 attrs
    # ------------------------------------------------------------------

    def _obj_addr(self, obj_num: int) -> int:
        """Byte address of the object entry for obj_num (1-indexed)."""
        base = self.object_table + self._obj_defaults_size
        return base + (obj_num - 1) * self._obj_entry_size

    def _get_obj_parent(self, obj_num: int) -> int:
        """Return the parent object number for obj_num, or 0 if none/invalid."""
        if obj_num < 1: return 0
        addr = self._obj_addr(obj_num)
        return self.memory[addr + 4] if self.version <= 3 else self.read_word(addr + 6)

    def _get_obj_sibling(self, obj_num: int) -> int:
        """Return the sibling object number for obj_num, or 0 if none/invalid."""
        if obj_num < 1: return 0
        addr = self._obj_addr(obj_num)
        return self.memory[addr + 5] if self.version <= 3 else self.read_word(addr + 8)

    def _get_obj_child(self, obj_num: int) -> int:
        """Return the first child object number for obj_num, or 0 if none/invalid."""
        if obj_num < 1: return 0
        addr = self._obj_addr(obj_num)
        return self.memory[addr + 6] if self.version <= 3 else self.read_word(addr + 10)

    def _set_obj_parent(self, obj_num: int, val: int) -> None:
        addr = self._obj_addr(obj_num)
        if self.version <= 3:
            self.memory[addr + 4] = val & 0xFF
        else:
            self.write_word(addr + 6, val)

    def _set_obj_sibling(self, obj_num: int, val: int) -> None:
        addr = self._obj_addr(obj_num)
        if self.version <= 3:
            self.memory[addr + 5] = val & 0xFF
        else:
            self.write_word(addr + 8, val)

    def _set_obj_child(self, obj_num: int, val: int) -> None:
        addr = self._obj_addr(obj_num)
        if self.version <= 3:
            self.memory[addr + 6] = val & 0xFF
        else:
            self.write_word(addr + 10, val)

    def _test_attr(self, obj_num: int, attr: int) -> bool:
        """Return True if object obj_num has attribute attr set.

        Attributes are stored MSB-first in the entry's leading bytes:
        V3: 4 bytes (attrs 0-31), V5: 6 bytes (attrs 0-47).
        Attribute 0 is bit 7 of byte 0; attribute N is at byte N//8 bit 7-(N%8).
        """
        if obj_num < 1 or attr > self._obj_max_attrs: return False
        addr = self._obj_addr(obj_num)
        byte_idx = attr // 8
        bit = 7 - (attr % 8)
        return bool(self.memory[addr + byte_idx] & (1 << bit))

    def _set_attr(self, obj_num: int, attr: int) -> None:
        """Set attribute attr on object obj_num."""
        if obj_num < 1 or attr > self._obj_max_attrs: return
        addr = self._obj_addr(obj_num)
        byte_idx = attr // 8
        bit = 7 - (attr % 8)
        self.memory[addr + byte_idx] |= (1 << bit)

    def _clear_attr(self, obj_num: int, attr: int) -> None:
        """Clear attribute attr on object obj_num."""
        if obj_num < 1 or attr > self._obj_max_attrs: return
        addr = self._obj_addr(obj_num)
        byte_idx = attr // 8
        bit = 7 - (attr % 8)
        self.memory[addr + byte_idx] &= ~(1 << bit)

    def _prop_header(self, addr: int) -> tuple[int, int, int]:
        """Parse a property entry header at addr.

        Returns (prop_num, data_len, header_bytes).
        Returns (0, 0, 0) when size byte is 0 (end-of-list sentinel).

        V3: always 1-byte header.
          pnum = bits 4-0; plen = (bits 7-5) + 1.
        V5: 1-byte header if bit 7 = 0; 2-byte header if bit 7 = 1.
          1-byte: pnum = bits 5-0; plen = (bit 6 ? 2 : 1).
          2-byte: pnum = byte0 bits 5-0; plen = byte1 bits 5-0 (0 → 64).
        """
        sb = self.memory[addr]
        if sb == 0:
            return 0, 0, 0
        if self.version <= 3:
            return sb & 0x1F, (sb >> 5) + 1, 1
        # V5+
        pnum = sb & 0x3F
        if sb & 0x80:
            sb2 = self.memory[addr + 1]
            plen = sb2 & 0x3F
            if plen == 0:
                plen = 64
            return pnum, plen, 2
        else:
            return pnum, ((sb >> 6) & 1) + 1, 1

    def _get_prop(self, obj_num: int, prop_num: int) -> int:
        """Return value of property prop_num for object obj_num.

        If the property is absent, returns the default value from the
        object table's default property words.
        """
        if obj_num < 1: return 0
        addr = self._obj_addr(obj_num)
        prop_addr = self.read_word(addr + self._obj_prop_ptr_off)
        # Skip the object name Z-string (byte 0 = length in words)
        name_len = self.memory[prop_addr] * 2
        prop_addr += 1 + name_len
        while True:
            pnum, plen, hdr = self._prop_header(prop_addr)
            if hdr == 0:
                # Not found: return default property value
                if 1 <= prop_num <= self._obj_max_props:
                    return self.read_word(self.object_table + (prop_num - 1) * 2)
                return 0
            if pnum == prop_num:
                if plen == 1:
                    return self.memory[prop_addr + hdr]
                else:
                    return self.read_word(prop_addr + hdr)
            prop_addr += hdr + plen

    def _get_prop_addr(self, obj_num: int, prop_num: int) -> int:
        """Return byte address of property prop_num's data for obj_num, or 0.

        Returns the address of the first data byte (after the header bytes).
        Returns 0 if the property is not present.
        """
        if obj_num < 1: return 0
        addr = self._obj_addr(obj_num)
        prop_addr = self.read_word(addr + self._obj_prop_ptr_off)
        name_len = self.memory[prop_addr] * 2
        prop_addr += 1 + name_len
        while True:
            pnum, plen, hdr = self._prop_header(prop_addr)
            if hdr == 0:
                return 0
            if pnum == prop_num:
                return prop_addr + hdr
            prop_addr += hdr + plen

    def _get_next_prop(self, obj_num: int, prop_num: int) -> int:
        """Return the property number after prop_num for obj_num.

        If prop_num == 0, returns the first (highest-numbered) property.
        Returns 0 if there are no further properties.
        Properties are stored in descending order of property number.
        """
        if obj_num < 1: return 0
        addr = self._obj_addr(obj_num)
        prop_addr = self.read_word(addr + self._obj_prop_ptr_off)
        name_len = self.memory[prop_addr] * 2
        prop_addr += 1 + name_len
        while True:
            pnum, plen, hdr = self._prop_header(prop_addr)
            if hdr == 0: return 0
            if prop_num == 0 or pnum < prop_num:
                return pnum
            prop_addr += hdr + plen

    def _insert_obj(self, obj_num: int, dest: int) -> None:
        """Make obj_num the first child of dest (standard INSERT_OBJ semantics)."""
        self._remove_obj(obj_num)
        old_child = self._get_obj_child(dest)
        self._set_obj_parent(obj_num, dest)
        self._set_obj_sibling(obj_num, old_child)
        self._set_obj_child(dest, obj_num)

    def _remove_obj(self, obj_num: int) -> None:
        """Remove obj_num from its parent's child list.

        Walks the parent's child-sibling chain to find and unlink obj_num,
        then clears obj_num's parent and sibling fields.
        Does nothing if obj_num has no parent (already detached).
        """
        parent = self._get_obj_parent(obj_num)
        if parent == 0:
            return
        sibling = self._get_obj_sibling(obj_num)
        child = self._get_obj_child(parent)
        if child == obj_num:
            self._set_obj_child(parent, sibling)
        else:
            while child != 0:
                next_sib = self._get_obj_sibling(child)
                if next_sib == obj_num:
                    self._set_obj_sibling(child, sibling)
                    break
                child = next_sib
        self._set_obj_parent(obj_num, 0)
        self._set_obj_sibling(obj_num, 0)

    # ------------------------------------------------------------------
    # PUT_PROP and READ helpers — used by _dispatch_var opcodes 0x03/0x04
    # ------------------------------------------------------------------

    def _put_prop(self, obj_num: int, prop_num: int, val: int) -> None:
        """Write val to property prop_num of obj_num (PUT_PROP opcode, VAR 0x03).

        Searches the object's property list for the matching property number.
        If found and the property is 1 byte, writes the low byte of val.
        If 2 bytes (or more), writes the full 16-bit word.
        If the property is absent, the write is silently ignored (per V3 spec §12.5).

        Property table layout (each entry):
          byte 0: size byte — high 3 bits = (length - 1), low 5 bits = property number
          bytes 1+: property data (1 or 2 bytes for V3)
        """
        if obj_num < 1:
            return
        addr = self._obj_addr(obj_num)
        prop_addr = self.read_word(addr + self._obj_prop_ptr_off)
        # Skip the object name: byte 0 = name length in Z-string words
        name_len = self.memory[prop_addr] * 2
        prop_addr += 1 + name_len
        while True:
            pnum, plen, hdr = self._prop_header(prop_addr)
            if hdr == 0:
                return  # property not present — ignore write (spec §12.5)
            if pnum == prop_num:
                if plen == 1:
                    self.memory[prop_addr + hdr] = val & 0xFF
                else:
                    self.write_word(prop_addr + hdr, val)
                return
            prop_addr += hdr + plen

    def _encode_zword(self, word: str) -> tuple[int, ...]:
        """Encode a word as a Z-machine dictionary key.

        V3:   4 bytes = 2 words = 6 Z-chars packed as (w1, w2)
        V5+:  6 bytes = 3 words = 9 Z-chars packed as (w1, w2, w3)
        Each word packs 3 Z-chars (5 bits each) into bits 14-10, 9-5, 4-0.
        The last word has bit 15 set (end-of-string marker per spec §3.7).
        Words shorter than n_chars are padded with Z-char 5.

        Only lowercase A0 alphabet characters are handled; others map to code 5.
        """
        n_chars = 6 if self.version <= 3 else 9  # 6 Z-chars (V3) or 9 Z-chars (V5+)
        codes: list[int] = []
        for c in word[:n_chars]:
            if 'a' <= c <= 'z':
                codes.append(6 + (ord(c) - ord('a')))
            else:
                codes.append(5)  # padding for non-alpha
        while len(codes) < n_chars:
            codes.append(5)

        # Pack into 16-bit words, 3 Z-chars per word
        n_words = n_chars // 3
        words = []
        for i in range(n_words):
            c0, c1, c2 = codes[i * 3], codes[i * 3 + 1], codes[i * 3 + 2]
            words.append((c0 << 10) | (c1 << 5) | c2)
        words[-1] |= 0x8000  # end-of-string marker on last word
        return tuple(words)

    def _lookup_dictionary(self, word: str) -> int:
        """Look up a word in the game's dictionary; return its byte address or 0.

        Returns 0 immediately if no dictionary is present (dictionary_addr is 0).

        Uses binary search on the sorted dictionary (Z-machine spec §13 requires
        dictionary entries to be in sorted order of their encoded Z-string keys).

        Dictionary layout (at self.dictionary_addr):
          byte 0:    n = number of word-separator characters
          bytes 1..n: separator character codes
          byte n+1:  entry_length (bytes per entry; 7 for V3, 9 for V5)
          2 bytes:   num_entries (big-endian)
          entries:   sorted by their Z-string key (4 bytes for V3, 6 bytes for V5)

        Returns the byte address of the matching entry, or 0 if not found.
        """
        if not self.dictionary_addr:
            return 0
        d = self.dictionary_addr
        num_sep = self.memory[d]
        entry_len = self.memory[d + 1 + num_sep]
        num_entries = self.read_word(d + 2 + num_sep)
        entries_base = d + 2 + num_sep + 2  # byte address of first entry

        # Encode the lookup word; result length depends on version (2 or 3 words)
        target = self._encode_zword(word.lower())
        n_key_words = len(target)  # 2 for V3, 3 for V5+

        # Binary search over the sorted entries
        lo, hi = 0, num_entries - 1
        while lo <= hi:
            mid = (lo + hi) // 2
            entry_addr = entries_base + mid * entry_len
            entry = tuple(self.read_word(entry_addr + i * 2) for i in range(n_key_words))
            if entry == target:
                return entry_addr  # found
            # Numeric comparison for ordering (treat key as big-endian integer)
            entry_key  = 0
            target_key = 0
            for ew, tw in zip(entry, target):
                entry_key  = (entry_key  << 16) | ew
                target_key = (target_key << 16) | tw
            if target_key < entry_key:
                hi = mid - 1
            else:
                lo = mid + 1
        return 0  # not found

    def _do_read(self, text_buf: int, parse_buf: int) -> None:
        """READ opcode handler (SREAD, VAR 0x04): populate game input buffers.

        If self.input_command is empty (falsy), sets waiting_for_input = True and
        returns immediately without consuming anything. The caller must set
        self.input_command and call interpret() again to resume — the interpret()
        loop will re-enter this handler once input is available.

        When input IS present, clears waiting_for_input, consumes self.input_command,
        writes the command text into the Z-machine's text buffer, and tokenizes
        it into the parse buffer.

        Text buffer layout (Z-machine spec §8.4.3, V3):
          byte 0:   max chars the game can accept (set by the game, read-only here)
          bytes 1+: command bytes as lowercase ASCII, null-terminated
          (Note: V3 has NO length byte at offset 1; text starts immediately at byte 1.
           V5+ added a length byte at offset 1 and moved text to byte 2+.
           We follow the V3 layout here.)

        Parse buffer layout:
          byte 0:   max tokens the game can accept (set by the game, read-only here)
          byte 1:   number of tokens found (we write this)
          entries of 4 bytes each:
            bytes 0-1: dictionary address (big-endian word); 0 = word not in dictionary
            byte 2:    length of the word in characters
            byte 3:    position of word's first character in the text buffer (1-indexed,
                       counting from text_buf byte 0; first text byte is at position 1)

        We look up each word in the game's dictionary and write the matching entry address
        into the parse buffer. Words not found get address 0, which Zork handles gracefully
        by printing "I don't know the word <word>."

        Token positions use a search_start cursor that advances past each found word,
        so repeated identical words (e.g. "put egg in egg") get correct offsets.
        """
        if not self.input_command:
            # No input available: back up PC to the start of this READ instruction
            # so it will be re-executed (not skipped) when input arrives.
            # _instr_pc is saved by interpret() before each _execute_one() call.
            self.pc = self._instr_pc
            self.waiting_for_input = True
            return
        self.waiting_for_input = False

        cmd = self.input_command.lower().strip()
        self.input_command = ""      # consume command so it is not reused

        # Echo the command to output, matching real terminal behavior
        self._print_str(cmd + "\n")

        # Write to text buffer (if address non-zero).
        # V3 layout: byte 0 = max chars; bytes 1+ = text + NUL.
        # V5 layout: byte 0 = max chars; byte 1 = actual length (we write this);
        #            bytes 2+ = text (NOT null-terminated).
        max_chars = self.memory[text_buf] if text_buf > 0 else 200
        cmd_bytes = cmd.encode("ascii", errors="replace")[:max_chars]
        text_start = 1 if self.version <= 3 else 2   # offset from text_buf where text begins
        if text_buf > 0:
            for i, b in enumerate(cmd_bytes):
                self.memory[text_buf + text_start + i] = b
            if self.version <= 3:
                self.memory[text_buf + text_start + len(cmd_bytes)] = 0  # NUL-terminate
            else:
                self.memory[text_buf + 1] = len(cmd_bytes)  # V5 length byte

        # Tokenize into parse buffer (up to 8 space-delimited tokens).
        tokens = cmd.split()[:8]
        if parse_buf > 0:
            self.memory[parse_buf + 1] = len(tokens)
            search_start = 0
            for i, word in enumerate(tokens):
                offset = parse_buf + 2 + i * 4
                dict_addr = self._lookup_dictionary(word)
                self.write_word(offset, dict_addr)
                self.memory[offset + 2] = len(word)
                # Position: byte offset of word's first char from text_buf[0].
                # V3: text starts at text_buf[1] → first char position = 1.
                # V5: text starts at text_buf[2] → first char position = 2.
                pos = cmd.find(word, search_start)
                if pos >= 0:
                    search_start = pos + len(word)
                self.memory[offset + 3] = (pos + text_start) if pos >= 0 else 0
