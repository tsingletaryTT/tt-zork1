# tests/test_engines.py
import pytest
from pathlib import Path

GAME = "game/zork1.z3"

def test_sim_engine_startup_returns_zork_title():
    from engines.sim import SimEngine
    eng = SimEngine(GAME)
    out = eng.startup()
    eng.close()
    assert "ZORK" in out

def test_sim_engine_step_open_mailbox():
    from engines.sim import SimEngine
    eng = SimEngine(GAME)
    eng.startup()
    out = eng.step("open mailbox")
    eng.close()
    assert "mailbox" in out.lower() or "leaflet" in out.lower()

def test_sim_engine_step_invalid_command_returns_string():
    from engines.sim import SimEngine
    eng = SimEngine(GAME)
    eng.startup()
    out = eng.step("xyzzy frobozz")
    eng.close()
    assert isinstance(out, str)
    assert len(out) > 0

def test_sim_engine_context_manager():
    from engines.sim import SimEngine
    with SimEngine(GAME) as eng:
        out = eng.startup()
    assert "ZORK" in out
