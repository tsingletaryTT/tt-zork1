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

def test_zstring_decode_known_abbreviation():
    """First abbreviation in zork1.z3's table decodes to 'the '."""
    from ttlang.zmachine_v3 import ZMachineV3
    zm = ZMachineV3(GAME_FILE.read_bytes())
    # zork1.z3 abbreviation table at 0x01F0; entry 0 is word address → byte addr
    word_addr = zm.read_word(zm.abbrev_table)
    text = zm.decode_zstring(word_addr * 2)
    assert text == "the ", f"Got: {text!r}"

def test_abbreviation_west_of_house():
    """Object 64 in zork1.z3 is 'West of House' — tests abbreviation lookup."""
    from ttlang.zmachine_v3 import ZMachineV3
    zm = ZMachineV3(GAME_FILE.read_bytes())
    # Object 64 name should contain 'West' when decoded
    name = zm.get_object_name(64)
    assert "West" in name or "west" in name.lower(), f"Got: {name!r}"
