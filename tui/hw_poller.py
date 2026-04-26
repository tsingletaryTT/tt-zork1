# tui/hw_poller.py
"""Poll Tenstorrent hardware telemetry via `tt-smi -s`.

HardwareSnapshot holds one reading. poll() never raises — callers get a
fully-zeroed snapshot (all -1) when hardware is unavailable.

The stage_label field is NOT set by poll(); it is filled in by ZMachineTuiApp
before posting a HardwareUpdate message, since the app knows which engine is active.

Parsing notes:
  ASIC_TEMPERATURE — 24-bit hex value; top byte is degrees Celsius.
    e.g. 0x26ef22 >> 16 = 0x26 = 38°C
  TDP — raw hex integer in watts.
    e.g. 0x11 = 17W
  Tensix/RISC-V utilisation and DRAM bandwidth are not present in the
  basic `tt-smi -s` snapshot; those fields are always returned as -1.
"""
from __future__ import annotations

import json
import subprocess
from dataclasses import dataclass, replace


@dataclass
class HardwareSnapshot:
    """One telemetry reading from a Tenstorrent ASIC.

    Fields set to -1 indicate that the value is unavailable (either because
    tt-smi failed, or because the metric is not reported by the basic snapshot).
    """

    tensix_pct: float       # Tensix core utilisation 0–100; -1 if unavailable
    riscv_pct: float        # RISC-V core utilisation 0–100; -1 if unavailable
    dram_read_gbps: float   # DRAM read bandwidth in GB/s; -1 if unavailable
    dram_write_gbps: float  # DRAM write bandwidth in GB/s; -1 if unavailable
    temp_c: float           # ASIC temperature in degrees Celsius; -1 if unavailable
    power_w: float          # Board TDP in watts; -1 if unavailable
    stage_label: str        # Filled by ZMachineTuiApp, NOT by poll()


# Sentinel used as the base for all zeroed/unavailable snapshots.
# We use replace() to produce independent instances so callers cannot mutate
# the sentinel by accident.
_ZEROED = HardwareSnapshot(
    tensix_pct=-1.0,
    riscv_pct=-1.0,
    dram_read_gbps=-1.0,
    dram_write_gbps=-1.0,
    temp_c=-1.0,
    power_w=-1.0,
    stage_label="",
)


def poll() -> HardwareSnapshot:
    """Run `tt-smi -s`, parse JSON, return a HardwareSnapshot.

    Returns a zeroed snapshot with all numeric fields set to -1 if:
    - tt-smi is not installed (FileNotFoundError)
    - tt-smi returns a non-zero exit code
    - the output cannot be parsed as JSON
    - any other exception occurs during execution or parsing

    Never raises.
    """
    try:
        result = subprocess.run(
            ["tt-smi", "-s"],
            capture_output=True,
            text=True,
            timeout=3,
        )
        if result.returncode != 0:
            return replace(_ZEROED)
        data = json.loads(result.stdout)
        return _parse(data)
    except Exception:
        # Catches FileNotFoundError, TimeoutExpired, JSONDecodeError, and any
        # other unexpected exception so that poll() never raises to the caller.
        return replace(_ZEROED)


def _parse(data: dict) -> HardwareSnapshot:
    """Extract fields from the tt-smi -s JSON structure.

    Expected structure::

        {
          "device_info": [{
            "smbus_telem": {
              "ASIC_TEMPERATURE": "0x26ef22",
              "TDP": "0x11",
              "AICLK": "0x320"
            }
          }]
        }

    Only the first device entry is read. Returns a zeroed snapshot on any
    structural mismatch or parsing error.
    """
    try:
        devices = data.get("device_info", [])
        if not devices:
            return replace(_ZEROED)
        telem = devices[0].get("smbus_telem", {})

        temp_c = _hex_to_temp(telem.get("ASIC_TEMPERATURE"))
        power_w = _hex_int(telem.get("TDP"))

        return HardwareSnapshot(
            tensix_pct=-1.0,
            riscv_pct=-1.0,
            dram_read_gbps=-1.0,
            dram_write_gbps=-1.0,
            temp_c=temp_c,
            power_w=power_w,
            stage_label="",
        )
    except Exception:
        return replace(_ZEROED)


def _hex_int(value) -> float:
    """Parse a hex string such as '0x11' to a float.

    Returns -1.0 on any parse failure (None, empty string, non-hex text).
    """
    try:
        return float(int(str(value), 16))
    except (TypeError, ValueError):
        return -1.0


def _hex_to_temp(value) -> float:
    """Extract temperature in °C from the ASIC_TEMPERATURE raw hex value.

    The Tenstorrent ASIC_TEMPERATURE telemetry field is a 24-bit value whose
    top byte is the temperature in degrees Celsius.

    Example: 0x26ef22 >> 16 = 0x26 = 38°C

    Returns -1.0 on any parse failure.
    """
    try:
        raw = int(str(value), 16)
        return float((raw >> 16) & 0xFF)
    except (TypeError, ValueError):
        return -1.0
