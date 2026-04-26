# tui/hw_poller.py
"""Poll Tenstorrent hardware telemetry via sysfs or `tt-smi -s`.

HardwareSnapshot holds one reading. poll() never raises — callers get a
fully-zeroed snapshot (all -1) when hardware is unavailable.

The stage_label field is NOT set by poll(); it is filled in by ZMachineTuiApp
before posting a HardwareUpdate message, since the app knows which engine is active.

Polling strategy (sysfs-first):
  _poll_sysfs() reads directly from the kernel hwmon driver — no subprocess,
  microseconds instead of ~150 ms.  If the sysfs paths don't exist (e.g. no
  Tenstorrent card, or the driver isn't loaded), it returns a fully -1 snapshot
  and poll() falls back to tt-smi.

  sysfs paths (glob-expanded):
    /sys/bus/pci/drivers/tenstorrent/*/hwmon/hwmon*/temp1_input
        → millidegrees C  (e.g. 61108 → 61.1°C)
    /sys/bus/pci/drivers/tenstorrent/*/hwmon/hwmon*/power1_input
        → microwatts      (e.g. 71000000 → 71 W)

tt-smi fallback parsing notes (when sysfs unavailable):
  ASIC_TEMPERATURE — 24-bit hex value; top byte is degrees Celsius.
    e.g. 0x26ef22 >> 16 = 0x26 = 38°C
  TDP — raw hex integer in watts.
    e.g. 0x11 = 17W
  Tensix/RISC-V utilisation and DRAM bandwidth are not present in the
  basic `tt-smi -s` snapshot; those fields are always returned as -1.
"""
import glob
import json
import subprocess
from dataclasses import dataclass, replace
from pathlib import Path


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


def _poll_sysfs() -> HardwareSnapshot:
    """Read temp and power directly from the tenstorrent hwmon sysfs driver.

    Uses glob to locate the hwmon directory under the PCI driver path.  Only
    the first discovered device is read (multi-card systems are not currently
    supported here — tt-smi fallback handles those cases if needed).

    Returns a zeroed snapshot (all fields -1) when:
    - The tenstorrent PCI driver is not loaded.
    - No hwmon entries are present (card not powered or driver not initialised).
    - Any read or parse failure occurs.

    Never raises.
    """
    try:
        # Glob for temp1_input under any TT PCI device's hwmon directory.
        temp_files = glob.glob(
            "/sys/bus/pci/drivers/tenstorrent/*/hwmon/hwmon*/temp1_input"
        )
        power_files = glob.glob(
            "/sys/bus/pci/drivers/tenstorrent/*/hwmon/hwmon*/power1_input"
        )

        if not temp_files and not power_files:
            # No sysfs entries — caller should fall back to tt-smi.
            return replace(_ZEROED)

        temp_c: float = -1.0
        power_w: float = -1.0

        if temp_files:
            # temp1_input is in millidegrees Celsius.
            raw_temp = Path(temp_files[0]).read_text().strip()
            temp_c = float(raw_temp) / 1000.0

        if power_files:
            # power1_input is in microwatts.
            raw_power = Path(power_files[0]).read_text().strip()
            power_w = float(raw_power) / 1_000_000.0

        return replace(_ZEROED, temp_c=temp_c, power_w=power_w)
    except Exception:
        # Any read/parse error → caller falls back to tt-smi.
        return replace(_ZEROED)


def poll() -> HardwareSnapshot:
    """Return a HardwareSnapshot, trying sysfs first and tt-smi as fallback.

    Strategy:
    1. Call _poll_sysfs().  If it returns a snapshot with at least one
       non-(-1) field, return it immediately (fast path, no subprocess).
    2. Otherwise fall through to the existing tt-smi subprocess call.

    Returns a zeroed snapshot with all numeric fields set to -1 if:
    - Neither sysfs nor tt-smi is available.
    - tt-smi is not installed (FileNotFoundError).
    - tt-smi returns a non-zero exit code.
    - the output cannot be parsed as JSON.
    - any other exception occurs during execution or parsing.

    Never raises.
    """
    # --- Fast path: sysfs hwmon driver ---
    sysfs_snap = _poll_sysfs()
    if sysfs_snap.temp_c != -1.0 or sysfs_snap.power_w != -1.0:
        # At least one metric was available from sysfs; use it directly.
        return sysfs_snap

    # --- Fallback: tt-smi subprocess ---
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

        return replace(_ZEROED, temp_c=temp_c, power_w=power_w)
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
