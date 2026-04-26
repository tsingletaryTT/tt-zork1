# tests/test_tui_vocabulary.py
"""Vocabulary extraction from the Z-machine dictionary table."""
from pathlib import Path
import pytest


GAME_PATH = Path("game/zork1.z3")


def test_extract_vocabulary_returns_nonempty_frozenset():
    from ttlang.zmachine_v3 import ZMachineV3
    from tui.vocabulary import extract_vocabulary

    zm = ZMachineV3(GAME_PATH.read_bytes())
    vocab = extract_vocabulary(zm)

    assert isinstance(vocab, frozenset)
    assert len(vocab) > 100


def test_extract_vocabulary_contains_known_words():
    from ttlang.zmachine_v3 import ZMachineV3
    from tui.vocabulary import extract_vocabulary

    zm = ZMachineV3(GAME_PATH.read_bytes())
    vocab = extract_vocabulary(zm)

    assert "mailbox" in vocab
    assert "north" in vocab


def test_extract_vocabulary_returns_empty_frozenset_on_error():
    from tui.vocabulary import extract_vocabulary

    class BrokenZM:
        dictionary_addr = 0

        def read_byte(self, addr: int) -> int:
            raise RuntimeError("boom")

        def read_word(self, addr: int) -> int:
            raise RuntimeError("boom")

        def decode_zstring(self, addr: int) -> str:
            return ""

    assert extract_vocabulary(BrokenZM()) == frozenset()
