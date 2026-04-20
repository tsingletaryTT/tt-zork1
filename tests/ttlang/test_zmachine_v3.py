# tests/ttlang/test_zmachine_v3.py
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).parent.parent.parent))
sys.path.insert(0, str(pathlib.Path(__file__).parent.parent))

import pytest

GAME_FILE = pathlib.Path(__file__).parent.parent.parent / "game" / "zork1.z3"

def test_header_version():
    from ttlang.zmachine_v3 import ZMachineV3
    zm = ZMachineV3(GAME_FILE.read_bytes())
    assert zm.version == 3

def test_header_initial_pc():
    from ttlang.zmachine_v3 import ZMachineV3
    zm = ZMachineV3(GAME_FILE.read_bytes())
    # Initial PC for zork1.z3 is at header bytes 0x06-0x07
    assert zm.pc > 0
    assert zm.pc < len(zm.memory)

def test_header_abbreviation_table():
    from ttlang.zmachine_v3 import ZMachineV3
    zm = ZMachineV3(GAME_FILE.read_bytes())
    # Abbreviation table address at header 0x18-0x19; zork1.z3 value is 0x01F0
    assert zm.abbrev_table == 0x01F0

def test_header_global_vars():
    from ttlang.zmachine_v3 import ZMachineV3
    zm = ZMachineV3(GAME_FILE.read_bytes())
    assert zm.global_vars_addr > 0
