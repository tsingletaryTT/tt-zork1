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
            self.stack.append(value)
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

        This is the public entry point. Handles up to 60 16-bit words
        (sufficient for any Zork I string) and starts at recursion depth 0.
        Returns plain text with abbreviations expanded.
        """
        return self._decode_zstring_raw(addr, 60, 0)

    def get_object_name(self, obj_num: int) -> str:
        """Return the text name of Z-machine object obj_num (1-indexed).

        V3 object table layout (Z-machine spec §12):
          - 31 × 2-byte default property values (62 bytes)
          - Then one 9-byte entry per object:
              bytes 0-3: attribute flags (32 bits)
              byte  4:   parent object number
              byte  5:   sibling object number
              byte  6:   child object number
              bytes 7-8: property table address (big-endian word)

        The property table for each object starts with:
          byte 0:   length of object name in Z-string words
          bytes 1+: the Z-string name

        Returns empty string if obj_num is out of range or has no name.
        """
        if obj_num < 1 or obj_num > 255:
            return ""

        # Locate the object's 9-byte entry in the object table
        default_props_size = 31 * 2  # 31 words of default property values
        obj_base = self.object_table + default_props_size + (obj_num - 1) * 9

        # Read the property table pointer from bytes 7-8 of the entry
        prop_ptr = self.read_word(obj_base + 7)
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
        """Append a single character to the output buffer."""
        self.output.append(c)

    def _print_str(self, s: str) -> None:
        """Append a string to the output buffer."""
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
            self.set_var(store_var, 0)
            return

        routine_addr = packed_addr * 2  # V3: word address → byte address
        num_locals = self.memory[routine_addr]
        routine_addr += 1

        # Read default local values (one word each)
        locals_: list[int] = []
        for i in range(num_locals):
            locals_.append(self.read_word(routine_addr))
            routine_addr += 2

        # Override with supplied arguments
        for i, arg in enumerate(args):
            if i < len(locals_):
                locals_[i] = arg

        # Push call frame and jump into routine body
        self.frames.append({
            "ret_pc": self.pc,
            "locals": locals_,
            "store_var": store_var,
            "num_locals": num_locals,
        })
        self.pc = routine_addr

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
        """
        for _ in range(max_instructions):
            if not self.running:
                break
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

        # EXT form (V5+, not V3) — treat as NOP to avoid garbling PC
        if opcode_byte == 0xBE:
            self.code_byte()
            return

        form = opcode_byte >> 6          # 0b11=VAR, 0b10=short, else long

        if form == 0b11:
            # VAR form: opcode in low 5 bits; operand-types byte follows
            opcode = opcode_byte & 0x1F
            operands = self._load_var_operands()
            self._dispatch_var(opcode, operands)

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
        """Dispatch a 2OP instruction. Stub — implemented in Task 5."""
        pass

    def _dispatch_1op(self, opcode: int, a: int) -> None:
        """Dispatch a 1OP instruction. Stub — implemented in Task 5."""
        pass

    def _dispatch_0op(self, opcode: int) -> None:
        """Dispatch a 0OP instruction. Stub — implemented in Task 5."""
        pass

    def _dispatch_var(self, opcode: int, operands: list[int]) -> None:
        """Dispatch a VAR-form instruction. Stub — implemented in Task 5."""
        pass
