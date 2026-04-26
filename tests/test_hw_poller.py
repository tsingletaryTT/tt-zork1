# tests/test_hw_poller.py
import json
import pytest
from unittest.mock import patch, MagicMock


_SAMPLE_JSON = {
    "device_info": [{
        "smbus_telem": {
            "ASIC_TEMPERATURE": "0x26ef22",
            "TDP": "0x11",
            "AICLK": "0x320",
        }
    }]
}


def test_poll_extracts_temperature():
    from tui.hw_poller import poll

    with patch("tui.hw_poller.subprocess.run") as mock_run:
        mock_run.return_value = MagicMock(returncode=0, stdout=json.dumps(_SAMPLE_JSON))
        snap = poll()

    # 0x26ef22 >> 16 = 0x26 = 38
    assert snap.temp_c == pytest.approx(38.0)


def test_poll_extracts_power():
    from tui.hw_poller import poll

    with patch("tui.hw_poller.subprocess.run") as mock_run:
        mock_run.return_value = MagicMock(returncode=0, stdout=json.dumps(_SAMPLE_JSON))
        snap = poll()

    # 0x11 = 17
    assert snap.power_w == pytest.approx(17.0)


def test_poll_returns_zeroed_when_tt_smi_missing():
    from tui.hw_poller import poll

    with patch("tui.hw_poller.subprocess.run", side_effect=FileNotFoundError):
        snap = poll()

    assert snap.temp_c == -1
    assert snap.tensix_pct == -1
    assert snap.power_w == -1


def test_poll_returns_zeroed_on_nonzero_exit():
    from tui.hw_poller import poll

    with patch("tui.hw_poller.subprocess.run") as mock_run:
        mock_run.return_value = MagicMock(returncode=1, stdout="")
        snap = poll()

    assert snap.temp_c == -1


def test_poll_returns_zeroed_on_malformed_json():
    from tui.hw_poller import poll

    with patch("tui.hw_poller.subprocess.run") as mock_run:
        mock_run.return_value = MagicMock(returncode=0, stdout="not json {")
        snap = poll()

    assert snap.temp_c == -1


def test_poll_never_raises():
    from tui.hw_poller import poll

    with patch("tui.hw_poller.subprocess.run", side_effect=PermissionError("denied")):
        snap = poll()  # must not raise

    assert snap.tensix_pct == -1
