# engines/device.py
"""
DeviceEngine — QB2 DRAM-backed Z-machine engine.

Stage 2 of the demo arc: the game binary is uploaded to Tenstorrent Blackhole
DRAM as a ttnn tensor and verified via round-trip before the Python Z-machine
interpreter runs against the DRAM-resident bytes. This confirms the data path
that the full on-chip RISC-V kernel (Stage 3) will reuse.

Hardware requirement: /dev/tenstorrent must be present (QB2 Blackhole card).
"""
from __future__ import annotations

import torch
import ttnn
from pathlib import Path

from engines.base import BaseEngine
from ttlang.zmachine_v3 import ZMachineV3

# Number of Z-machine instructions to execute during game initialization.
# Zork I's init phase typically runs 2 000–5 000 instructions before the
# first READ opcode; 10 000 gives generous headroom.
INSTRUCTIONS_STARTUP = 10000

# Per-turn execution budget.  A typical Zork command costs 200–500 instructions;
# 5 000 is safe for complex moves without risking a firmware watchdog timeout.
INSTRUCTIONS_PER_TURN = 5000


def _game_to_device(game_bytes: bytes, device: ttnn.Device) -> ttnn.Tensor:
    """Upload raw game bytes to QB2 DRAM and return a device tensor.

    ttnn ROW_MAJOR_LAYOUT with bfloat16 requires an even element count
    (minimum 2-element row), so a zero byte is appended when the payload
    has odd length.  The Z-machine interpreter never reads past the
    original game_bytes length, so the pad byte is harmless.

    Each byte is stored as a bfloat16 float so it survives the
    to_torch() round-trip without loss (0–255 are all exact in bfloat16).
    """
    padded = bytearray(game_bytes)
    if len(padded) % 2:
        padded.append(0)
    # frombuffer needs a copy (clone) so the underlying bytearray stays alive.
    t = torch.frombuffer(bytes(padded), dtype=torch.uint8).clone().float()
    return ttnn.from_torch(
        t,
        dtype=ttnn.bfloat16,
        layout=ttnn.ROW_MAJOR_LAYOUT,
        device=device,
        memory_config=ttnn.DRAM_MEMORY_CONFIG,
    )


def _device_to_bytes(tensor: ttnn.Tensor) -> bytearray:
    """Download a DRAM tensor back to a host bytearray.

    float(v) converts each tensor scalar to a Python float before rounding
    to guard against object-type mismatches across ttnn versions.
    & 0xFF is a belt-and-suspenders guard in case dtype semantics change.
    """
    t = ttnn.to_torch(tensor)
    return bytearray(int(round(float(v))) & 0xFF for v in t.tolist())


class DeviceEngine(BaseEngine):
    """QB2 DRAM-backed engine. Game binary lives on Tenstorrent silicon.

    Game data is uploaded to QB2 DRAM as a ttnn tensor and verified
    via round-trip before running the Python Z-machine interpreter
    against the DRAM-resident bytes. Stage 2 of the demo arc.

    Requires /dev/tenstorrent to be present.  Import and instantiation
    will raise ImportError or RuntimeError if hardware is unavailable.
    """

    label = "Stage 2 — Game data on QB2 DRAM (Tenstorrent hardware)"

    def __init__(self, game_path: str) -> None:
        super().__init__(game_path)
        game_bytes = Path(game_path).read_bytes()

        # Verify at least one device is visible before attempting to open it.
        # A topology error here means the cluster descriptor was generated for a
        # different hardware configuration — e.g. descriptor says P300x4 (8 chips)
        # but only P300x2 (4 chips) is installed.  Fix:
        #   source ~/code/tt-lang/build/env/activate
        #   python -c "import ttnn; ttnn.utils.generate_cluster_descriptor()"
        # or set TT_METAL_CLUSTER_DESCRIPTOR_PATH to a matching descriptor file.
        try:
            num_devices = ttnn.GetNumAvailableDevices()
        except Exception as e:
            raise RuntimeError(
                "ttnn topology error — cluster descriptor likely does not match hardware.\n"
                "  Chips detected vs descriptor mismatch (e.g. P300x2 where P300x4 expected).\n"
                "  Fix: regenerate the cluster descriptor:\n"
                "    source ~/code/tt-lang/build/env/activate\n"
                "    python -c \"import ttnn; ttnn.utils.generate_cluster_descriptor()\"\n"
                "  Or set: TT_METAL_CLUSTER_DESCRIPTOR_PATH=/path/to/descriptor.yaml\n"
                f"  (Original error: {e})"
            ) from e

        if num_devices < 1:
            raise RuntimeError(
                "No Tenstorrent devices found — is /dev/tenstorrent present?\n"
                "  Run: tt-smi -s  to confirm device visibility."
            )

        # Open the first Blackhole device (chip 0).
        self._device = ttnn.open_device(device_id=0)

        # Wrap all post-open initialization in try/except so that any failure
        # (DRAM upload error, round-trip mismatch, interpreter init crash, etc.)
        # closes the device handle before re-raising, preventing a handle leak.
        try:
            # Upload game binary to DRAM.
            tensor = _game_to_device(game_bytes, self._device)

            # Verify the round-trip: the first 8 header bytes must match exactly.
            # This catches DRAM page fragmentation or bfloat16 precision issues.
            dram_bytes = _device_to_bytes(tensor)
            assert dram_bytes[:8] == bytearray(game_bytes[:8]), "DRAM round-trip mismatch"

            # Record the DRAM buffer address for documentation / future use by
            # on-chip kernels that need the physical address of the game data.
            self._dram_addr = tensor.buffer_address()

            # Construct the interpreter from the verified DRAM-backed bytes.
            # The Python host runs the Z-machine logic; data originated on silicon.
            self._zm = ZMachineV3(bytes(dram_bytes))
        except Exception:
            ttnn.close_device(self._device)
            self._device = None
            raise

    def startup(self) -> str:
        """Run the game's initialization sequence and return the opening text.

        Returns an empty string if the game ended during initialization
        (unusual, but the interpreter marks running=False in that case).
        """
        self._zm.interpret(INSTRUCTIONS_STARTUP)
        return self._zm.flush_output()

    def step(self, command: str) -> str:
        """Submit one player command and return the game's response text.

        Returns an empty string if called after the game has ended
        (running=False), matching SimEngine's behaviour for consistency.
        """
        self._zm.input_command = command
        self._zm.interpret(INSTRUCTIONS_PER_TURN)
        return self._zm.flush_output()

    @property
    def game_over(self) -> bool:
        """True when the game has ended via death or victory."""
        return self._zm.game_over

    @property
    def running(self) -> bool:
        """False when the game has quit cleanly (QUIT opcode executed)."""
        return self._zm.running

    def close(self) -> None:
        """Close the QB2 device and release DRAM resources."""
        if self._device is not None:
            ttnn.close_device(self._device)
            self._device = None
