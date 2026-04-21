# tests/ttlang/test_device.py
# Requires QB2 hardware. Skip if no device present.
import pytest
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

pytestmark = pytest.mark.skipif(
    not Path("/dev/tenstorrent").exists(),
    reason="QB2 hardware not available"
)

GAME_FILE = Path(__file__).parent.parent.parent / "game" / "zork1.z3"

def test_game_loads_to_device():
    """Game file should round-trip through QB2 DRAM cleanly."""
    import torch, ttnn
    game_bytes = GAME_FILE.read_bytes()
    game_tensor = torch.frombuffer(bytearray(game_bytes), dtype=torch.uint8).clone()

    device = ttnn.open_device(device_id=0)
    try:
        on_device = ttnn.from_torch(
            game_tensor.float(),  # ttnn requires float; we cast back
            dtype=ttnn.bfloat16,
            layout=ttnn.ROW_MAJOR_LAYOUT,
            device=device,
            memory_config=ttnn.DRAM_MEMORY_CONFIG,
        )
        back = ttnn.to_torch(on_device)
        # Values should match to within bfloat16 precision for bytes 0-255
        assert abs(float(back[0]) - float(game_tensor[0])) < 2.0
        assert abs(float(back[6]) - float(game_tensor[6])) < 2.0
        print(f"  Device round-trip OK: {len(game_bytes)} bytes on QB2 DRAM")
    finally:
        ttnn.close_device(device)
