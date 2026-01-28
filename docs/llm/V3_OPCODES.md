# Z-Machine Version 3 Opcode Implementation Status

This document tracks our progress implementing all Z-machine V3 opcodes for the RISC-V interpreter.

**Goal:** Complete V3 implementation to run ANY V3 game (Zork trilogy, Hitchhiker's Guide, Leather Goddesses, Planetfall, etc.)

**Current Status:** 22/~100 opcodes implemented

## Opcode Categories

### 2OP Opcodes (Two Operands) - 28 total

| Opcode | Name | Status | Priority | Notes |
|--------|------|--------|----------|-------|
| 0x01 | JE (jump if equal) | ✅ DONE | Critical | |
| 0x02 | JL (jump if less) | ❌ TODO | High | Needed for comparisons |
| 0x03 | JG (jump if greater) | ❌ TODO | High | Needed for comparisons |
| 0x04 | DEC_CHK (decrement and check) | ✅ DONE | High | |
| 0x05 | INC_CHK (increment and check) | ❌ TODO | High | Common in loops |
| 0x06 | JIN (jump if object in object) | ❌ TODO | Medium | Object relationships |
| 0x07 | TEST (test flags) | ❌ TODO | Medium | Bitwise operations |
| 0x08 | OR (bitwise or) | ❌ TODO | Medium | Logic operations |
| 0x09 | AND (bitwise and) | ✅ DONE | Medium | |
| 0x0A | TEST_ATTR (test attribute) | ✅ DONE | High | Object attributes |
| 0x0B | SET_ATTR (set attribute) | ❌ TODO | High | Object modification |
| 0x0C | CLEAR_ATTR (clear attribute) | ❌ TODO | High | Object modification |
| 0x0D | STORE (store variable) | ✅ DONE | Critical | |
| 0x0E | INSERT_OBJ (insert object) | ❌ TODO | High | Object tree manipulation |
| 0x0F | LOADW (load word from array) | ❌ TODO | High | Memory access |
| 0x10 | LOADB (load byte from array) | ❌ TODO | High | Memory access |
| 0x11 | GET_PROP (get property) | ✅ DONE | High | |
| 0x12 | GET_PROP_ADDR (get property address) | ❌ TODO | Medium | Property access |
| 0x13 | GET_NEXT_PROP (get next property) | ❌ TODO | Medium | Property iteration |
| 0x14 | ADD (add) | ✅ DONE | High | |
| 0x15 | SUB (subtract) | ❌ TODO | High | Arithmetic |
| 0x16 | MUL (multiply) | ❌ TODO | Medium | Arithmetic |
| 0x17 | DIV (divide) | ❌ TODO | Medium | Arithmetic |
| 0x18 | MOD (modulo) | ❌ TODO | Low | Arithmetic |

### 1OP Opcodes (One Operand) - 16 total

| Opcode | Name | Status | Priority | Notes |
|--------|------|--------|----------|-------|
| 0x00 | JZ (jump if zero) | ✅ DONE | Critical | |
| 0x01 | GET_SIBLING | ✅ DONE | High | |
| 0x02 | GET_CHILD | ✅ DONE | High | |
| 0x03 | GET_PARENT | ✅ DONE | High | |
| 0x04 | GET_PROP_LEN | ❌ TODO | Medium | Property length |
| 0x05 | INC (increment) | ❌ TODO | High | Common operation |
| 0x06 | DEC (decrement) | ❌ TODO | High | Common operation |
| 0x07 | PRINT_ADDR (print string at address) | ❌ TODO | High | Text output |
| 0x08 | CALL_1S | ❌ TODO | Low | V4+ mostly |
| 0x09 | REMOVE_OBJ (remove object) | ❌ TODO | High | Object tree manipulation |
| 0x0A | PRINT_OBJ (print object name) | ❌ TODO | High | **CRITICAL for "You are in..."** |
| 0x0B | RET (return) | ✅ DONE | Critical | |
| 0x0C | JUMP (unconditional jump) | ❌ TODO | High | Control flow |
| 0x0D | PRINT_PADDR (print packed address) | ❌ TODO | Medium | Text output |
| 0x0E | LOAD (load variable) | ✅ DONE | Critical | |
| 0x0F | NOT (bitwise not) | ❌ TODO | Medium | V3: NOT, V4+: CALL_1N |

### 0OP Opcodes (No Operands) - 16 total

| Opcode | Name | Status | Priority | Notes |
|--------|------|--------|----------|-------|
| 0x00 | RTRUE (return true) | ✅ DONE | Critical | |
| 0x01 | RFALSE (return false) | ✅ DONE | Critical | |
| 0x02 | PRINT (print literal) | ✅ DONE | Critical | |
| 0x03 | PRINT_RET (print and return) | ✅ DONE | High | |
| 0x04 | NOP | ❌ TODO | Low | No operation |
| 0x05 | SAVE | ❌ TODO | Low | Save game state |
| 0x06 | RESTORE | ❌ TODO | Low | Restore game state |
| 0x07 | RESTART | ❌ TODO | Medium | Restart game |
| 0x08 | RET_POPPED | ❌ TODO | Medium | Return popped value |
| 0x09 | POP (V1-4) / CATCH (V5+) | ❌ TODO | Medium | Stack operation |
| 0x0A | QUIT | ❌ TODO | High | Exit game |
| 0x0B | NEW_LINE | ✅ DONE | High | |
| 0x0C | SHOW_STATUS | ❌ TODO | Medium | Status line (V3 only) |
| 0x0D | VERIFY | ❌ TODO | Low | Verify game file |
| 0x0F | PIRACY | ❌ TODO | Low | Anti-piracy check |

### VAR Opcodes (Variable Operands) - 32+ total

| Opcode | Name | Status | Priority | Notes |
|--------|------|--------|----------|-------|
| 0x00 | CALL (call routine) | ✅ DONE | Critical | |
| 0x01 | STOREW (store word) | ✅ DONE | High | |
| 0x02 | STOREB (store byte) | ❌ TODO | High | Memory write |
| 0x03 | PUT_PROP (put property) | ✅ DONE | High | |
| 0x04 | READ (read input) | ❌ TODO | **CRITICAL** | **Needed for gameplay!** |
| 0x05 | PRINT_CHAR (print character) | ❌ TODO | High | Character output |
| 0x06 | PRINT_NUM (print number) | ❌ TODO | **CRITICAL** | **Fixes "ixn" bug!** |
| 0x07 | RANDOM (random number) | ✅ DONE | Medium | Needs better impl |
| 0x08 | PUSH (push to stack) | ❌ TODO | Medium | Stack operation |
| 0x09 | PULL (pull from stack) | ❌ TODO | Medium | Stack operation |
| 0x0A | SPLIT_WINDOW | ❌ TODO | Low | V3: Screen control |
| 0x0B | SET_WINDOW | ❌ TODO | Low | V3: Screen control |
| 0x13 | OUTPUT_STREAM | ❌ TODO | Low | I/O redirection |
| 0x17 | SCAN_TABLE | ❌ TODO | Medium | Table search |

## Implementation Priority

### Phase 1: Critical Text Output (Fix "Release ixn" bug)
- [ ] PRINT_NUM - Print numbers (fixes release number display)
- [ ] PRINT_OBJ - Print object names ("West of House")
- [ ] PRINT_CHAR - Print individual characters
- [ ] PRINT_ADDR - Print string at address

### Phase 2: Critical Input (Enable Gameplay)
- [ ] READ - Read player input **MOST IMPORTANT**
- [ ] STOREB - Store bytes in memory

### Phase 3: Essential Object Operations
- [ ] SET_ATTR - Set object attributes
- [ ] CLEAR_ATTR - Clear object attributes
- [ ] INSERT_OBJ - Insert object into tree
- [ ] REMOVE_OBJ - Remove object from tree

### Phase 4: Essential Control Flow
- [ ] JUMP - Unconditional jump
- [ ] JL / JG - Less than / Greater than comparisons
- [ ] INC / DEC - Increment / Decrement
- [ ] INC_CHK - Increment and check

### Phase 5: Essential Arithmetic & Logic
- [ ] SUB - Subtraction
- [ ] OR - Bitwise OR
- [ ] TEST - Test flags
- [ ] NOT - Bitwise NOT

### Phase 6: Memory Operations
- [ ] LOADW / LOADB - Load word/byte from array
- [ ] GET_PROP_ADDR - Get property address

### Phase 7: Nice to Have
- [ ] QUIT - Clean exit
- [ ] RESTART - Restart game
- [ ] MUL / DIV / MOD - More arithmetic
- [ ] SAVE / RESTORE - Save game

## Testing Plan

After each phase, test with:
1. **Zork I** - Primary test case (what we're developing against)
2. **Hitchhiker's Guide to the Galaxy** - Complex puzzles, different mechanics
3. **Leather Goddesses of Phobos** - Different style, humor
4. **Planetfall** - Robot companion, different object interactions

## Notes

- V3 opcodes are sufficient for most Infocom games
- Some opcodes (SAVE/RESTORE) may need host support
- Screen control opcodes (SPLIT_WINDOW) are optional for basic gameplay
- Focus on text-mode gameplay first, enhance later
