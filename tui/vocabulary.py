# tui/vocabulary.py
"""Extract the Z-machine vocabulary from a loaded game binary.

Two sources are merged to produce the full vocabulary:

1. The Z-machine dictionary table (spec §13) — holds every word the parser
   recognises. In V3, each entry is a 4-byte Z-string key (two Z-machine words
   = 20 z-chars, encoding at most 6 lowercase ASCII chars). Words longer than 6
   characters are therefore stored truncated ("mailbo" for "mailbox"). These
   short forms are still useful for command highlighting (north, take, put …).

2. Object names from the object table (spec §12) — every in-game object carries
   a full Z-string name decoded without truncation (e.g. "small mailbox",
   "brass lantern", "lurking grue"). Splitting these multi-word names on
   whitespace yields the complete tokens colorizers need (mailbox, grue, …).

The two sets are unioned and returned as a frozenset of lowercase strings. If
anything goes wrong (malformed game file, unexpected format, missing tables),
frozenset() is returned so callers degrade gracefully without crashing.

Public API
----------
extract_vocabulary(zm) -> frozenset[str]
    zm must expose:
      .dictionary_addr  — byte address of the dictionary table (int)
      .read_byte(addr)  — read one byte from the game binary
      .read_word(addr)  — read a big-endian 16-bit word from the game binary
      .decode_zstring(addr) — decode a Z-string at the given byte address

    All of these are satisfied by ttlang.zmachine_v3.ZMachineV3.
"""
from __future__ import annotations

import re

# Minimum token length to include; filters out single-letter Z-chars
# and Z-machine punctuation abbreviations like "$ve" or "#rand".
_MIN_TOKEN_LEN = 2

# Punctuation that can stick to the edges of tokens in object names
# (e.g. "burned-out" → keep "burned" and "out" separately).
_STRIP_CHARS = re.compile(r"^[\W_]+|[\W_]+$")


def _scan_dictionary(zm) -> set[str]:
    """Return all words from the Z-machine dictionary table.

    Dictionary layout (Z-machine V3 spec §13):
        byte 0:     n = number of word-separator characters
        bytes 1..n: the separator characters themselves (e.g. '.', ',', '"')
        byte n+1:   entry_length — bytes per entry (V3 always 7)
        2 bytes:    entry_count — number of entries (big-endian)
        entry_count × entry_length bytes: sorted dictionary entries

    Each entry begins with a 4-byte Z-string key encoding up to 6 chars.
    We call decode_zstring() on each entry address to recover the text.
    """
    words: set[str] = set()

    addr = zm.dictionary_addr
    if not addr:
        return words  # no dictionary — safe early exit

    # Skip past the separator list: 1 count byte + n separator bytes
    sep_count = zm.read_byte(addr)
    addr += 1 + sep_count          # now points at entry_length byte

    entry_length = zm.read_byte(addr)
    addr += 1                      # now points at entry_count word
    if entry_length == 0:
        return words               # malformed — avoid divide-by-zero

    entry_count = zm.read_word(addr)
    addr += 2                      # now points at first entry

    for i in range(entry_count):
        entry_addr = addr + i * entry_length
        # decode_zstring returns the truncated-to-6-char key as a Python string
        word = zm.decode_zstring(entry_addr).strip().lower()
        if word and len(word) >= _MIN_TOKEN_LEN:
            words.add(word)

    return words


def _scan_object_names(zm) -> set[str]:
    """Return individual word tokens from all Z-machine object names.

    Object table layout (Z-machine V3 spec §12):
        62 bytes: 31 default property values (2 bytes each)
        Then, one 9-byte entry per object (objects numbered 1–255):
            bytes 0–3:  attribute flags (32 bits)
            byte  4:    parent object number
            byte  5:    sibling object number
            byte  6:    child object number
            bytes 7–8:  property table address (big-endian word)

    Each object's property table starts with:
        byte  0:    length of the object name in Z-string words (0 = no name)
        bytes 1+:   the Z-string name (decoded without truncation)

    Multi-word names like "small mailbox" or "lurking grue" are split on
    whitespace so that each constituent token ("mailbox", "grue") enters
    the vocabulary individually.

    The object table address is read from Z-machine header bytes 0x0A–0x0B.
    V3 allows at most 255 objects; we scan all 255 entries safely.
    """
    words: set[str] = set()

    # Header bytes 0x0A-0x0B hold the object table base address (spec §11)
    obj_table = zm.read_word(0x0A)
    if not obj_table:
        return words

    # The object table starts with 31 two-byte default property entries
    default_props_size = 31 * 2   # 62 bytes

    for obj_num in range(1, 256):  # V3 supports objects 1–255
        # Each object entry is 9 bytes; index from 0 means obj 1 is at offset 0
        obj_base = obj_table + default_props_size + (obj_num - 1) * 9
        prop_ptr = zm.read_word(obj_base + 7)   # bytes 7–8 of the entry
        if prop_ptr == 0:
            continue  # null property pointer — object doesn't exist

        # First byte of property table: name length in Z-string words
        name_len_words = zm.read_byte(prop_ptr)
        if name_len_words == 0:
            continue  # object exists but has no text name

        # Z-string name starts immediately at prop_ptr + 1
        name = zm.decode_zstring(prop_ptr + 1).strip().lower()
        if not name:
            continue

        # Split "small mailbox" → ["small", "mailbox"], strip edge punctuation
        for raw_token in name.split():
            token = _STRIP_CHARS.sub("", raw_token)
            if token and len(token) >= _MIN_TOKEN_LEN:
                words.add(token)

    return words


def extract_vocabulary(zm) -> frozenset[str]:
    """Read the Z-machine vocabulary from zm and return a frozenset of words.

    Combines words from two sources:
    - The game's dictionary table (parser vocabulary, truncated to 6 chars)
    - Object names from the object table (full names, split into tokens)

    zm must expose: .dictionary_addr (int), .read_byte(addr), .read_word(addr),
    .decode_zstring(addr) — all satisfied by ZMachineV3 from ttlang/zmachine_v3.py.

    Returns frozenset() on any error so callers degrade gracefully.
    """
    try:
        words = _scan_dictionary(zm)
        words |= _scan_object_names(zm)
        return frozenset(words)
    except Exception:
        # Malformed game, unsupported format, or stub raising intentionally —
        # return an empty set so the colorizer simply doesn't highlight anything.
        return frozenset()
