# tests/test_engines.py
import pytest
from pathlib import Path

# Resolve the game path relative to this file so tests work from any cwd.
GAME = str(Path(__file__).parent.parent / "game" / "zork1.z3")


@pytest.fixture
def sim(tmp_path):
    from engines.sim import SimEngine
    eng = SimEngine(GAME)
    yield eng
    eng.close()


def test_sim_engine_startup_returns_zork_title(sim):
    out = sim.startup()
    assert "ZORK" in out


def test_sim_engine_step_open_mailbox(sim):
    sim.startup()
    out = sim.step("open mailbox")
    assert "mailbox" in out.lower() or "leaflet" in out.lower()


def test_sim_engine_step_invalid_command_returns_string(sim):
    sim.startup()
    out = sim.step("xyzzy frobozz")
    assert isinstance(out, str)
    assert len(out) > 0


def test_sim_engine_context_manager():
    from engines.sim import SimEngine
    with SimEngine(GAME) as eng:
        out = eng.startup()
    assert "ZORK" in out
