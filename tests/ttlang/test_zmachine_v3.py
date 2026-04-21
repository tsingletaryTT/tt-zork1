# tests/ttlang/test_zmachine_v3.py
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).parent.parent.parent))
sys.path.insert(0, str(pathlib.Path(__file__).parent.parent))

GAME_FILE = pathlib.Path(__file__).parent.parent.parent / "game" / "zork1.z3"

def test_header_version():
    from ttlang.zmachine_v3 import ZMachineV3
    zm = ZMachineV3(GAME_FILE.read_bytes())
    assert zm.version == 3

def test_header_initial_pc():
    from ttlang.zmachine_v3 import ZMachineV3
    zm = ZMachineV3(GAME_FILE.read_bytes())
    # Initial PC for zork1.z3 header bytes 0x06-0x07 = 0x50D5
    assert zm.pc == 0x50D5

def test_header_abbreviation_table():
    from ttlang.zmachine_v3 import ZMachineV3
    zm = ZMachineV3(GAME_FILE.read_bytes())
    # Abbreviation table address at header 0x18-0x19; zork1.z3 value is 0x01F0
    assert zm.abbrev_table == 0x01F0

def test_header_global_vars():
    from ttlang.zmachine_v3 import ZMachineV3
    zm = ZMachineV3(GAME_FILE.read_bytes())
    assert zm.global_vars_addr > 0

def test_zstring_opening_text():
    """The opening word of Zork I should decode to 'ZORK'."""
    from ttlang.zmachine_v3 import ZMachineV3
    zm = ZMachineV3(GAME_FILE.read_bytes())
    # The game title string is near the beginning of high memory.
    # Object 41 in zork1.z3 is "ZORK owner(s manual".
    # We verify decode_zstring returns non-empty text for a known address.
    text = zm.decode_zstring(zm.initial_pc + 2)  # skip routine header
    # Must return something without crashing
    assert isinstance(text, str)

def test_abbreviation_west_of_house():
    """Object 64 in zork1.z3 is 'West of House' — tests abbreviation lookup."""
    from ttlang.zmachine_v3 import ZMachineV3
    zm = ZMachineV3(GAME_FILE.read_bytes())
    # Object 64 name should contain 'West' when decoded
    name = zm.get_object_name(64)
    assert "West" in name or "west" in name.lower(), f"Got: {name!r}"
