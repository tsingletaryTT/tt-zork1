# tests/test_hw_poller.py
import json
import pytest
from unittest.mock import patch, MagicMock, mock_open


_SAMPLE_JSON = {
    "device_info": [{
        "smbus_telem": {
            "ASIC_TEMPERATURE": "0x26ef22",
            "TDP": "0x11",
            "AICLK": "0x320",
        }
    }]
}

# Helper: make glob return no sysfs files so poll() falls through to tt-smi.
_NO_SYSFS = patch("tui.hw_poller.glob.glob", return_value=[])


def test_poll_extracts_temperature():
    from tui.hw_poller import poll

    with _NO_SYSFS, patch("tui.hw_poller.subprocess.run") as mock_run:
        mock_run.return_value = MagicMock(returncode=0, stdout=json.dumps(_SAMPLE_JSON))
        snap = poll()

    # 0x26ef22 >> 16 = 0x26 = 38
    assert snap.temp_c == pytest.approx(38.0)


def test_poll_extracts_power():
    from tui.hw_poller import poll

    with _NO_SYSFS, patch("tui.hw_poller.subprocess.run") as mock_run:
        mock_run.return_value = MagicMock(returncode=0, stdout=json.dumps(_SAMPLE_JSON))
        snap = poll()

    # 0x11 = 17
    assert snap.power_w == pytest.approx(17.0)


def test_poll_returns_zeroed_when_tt_smi_missing():
    from tui.hw_poller import poll

    with _NO_SYSFS, patch("tui.hw_poller.subprocess.run", side_effect=FileNotFoundError):
        snap = poll()

    assert snap.temp_c == -1
    assert snap.tensix_pct == -1
    assert snap.power_w == -1


def test_poll_returns_zeroed_on_nonzero_exit():
    from tui.hw_poller import poll

    with _NO_SYSFS, patch("tui.hw_poller.subprocess.run") as mock_run:
        mock_run.return_value = MagicMock(returncode=1, stdout="")
        snap = poll()

    assert snap.temp_c == -1


def test_poll_returns_zeroed_on_malformed_json():
    from tui.hw_poller import poll

    with _NO_SYSFS, patch("tui.hw_poller.subprocess.run") as mock_run:
        mock_run.return_value = MagicMock(returncode=0, stdout="not json {")
        snap = poll()

    assert snap.temp_c == -1


def test_poll_never_raises():
    from tui.hw_poller import poll

    with _NO_SYSFS, patch("tui.hw_poller.subprocess.run", side_effect=PermissionError("denied")):
        snap = poll()  # must not raise

    assert snap.tensix_pct == -1


# --- sysfs fast-path tests ---------------------------------------------------

def test_sysfs_reads_temperature():
    from tui.hw_poller import poll

    with patch("tui.hw_poller.glob.glob") as mock_glob, \
         patch("tui.hw_poller.Path") as mock_path:
        mock_glob.side_effect = lambda pattern: (
            ["/sys/.../temp1_input"] if "temp1" in pattern else []
        )
        mock_path.return_value.read_text.return_value = "61108\n"
        snap = poll()

    # 61108 millidegrees → 61.108°C
    assert snap.temp_c == pytest.approx(61.108)
    assert snap.power_w == -1.0  # no power file


def test_sysfs_reads_power():
    from tui.hw_poller import poll

    with patch("tui.hw_poller.glob.glob") as mock_glob, \
         patch("tui.hw_poller.Path") as mock_path:
        mock_glob.side_effect = lambda pattern: (
            ["/sys/.../power1_input"] if "power1" in pattern else []
        )
        mock_path.return_value.read_text.return_value = "71000000\n"
        snap = poll()

    # 71000000 microwatts → 71W
    assert snap.power_w == pytest.approx(71.0)
    assert snap.temp_c == -1.0  # no temp file


def test_sysfs_falls_back_to_ttsmi_on_no_files():
    """When no sysfs paths exist, poll() must call tt-smi."""
    from tui.hw_poller import poll

    with patch("tui.hw_poller.glob.glob", return_value=[]), \
         patch("tui.hw_poller.subprocess.run") as mock_run:
        mock_run.return_value = MagicMock(returncode=0, stdout=json.dumps(_SAMPLE_JSON))
        snap = poll()

    mock_run.assert_called_once()
    assert snap.temp_c == pytest.approx(38.0)
