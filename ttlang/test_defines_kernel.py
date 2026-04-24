"""
test_defines_kernel.py — Minimal test: single kernel + defines + NoC write via generic_op.

If "HELLO FROM RISC-V DEFINES TEST" appears in output: defines + single-kernel generic_op work.
If empty: the kernel never executed (dispatch failure or wrong processor).
"""
from __future__ import annotations
import sys, time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))
import torch
import ttnn

KERNEL_PATH = str(Path(__file__).parent.parent / "kernels" / "test_defines.cpp")
OUTPUT_SIZE = 256   # small buffer — just enough for the test message

_CORE = ttnn.CoreCoord(0, 0)
_CORE_RANGES = ttnn.CoreRangeSet([ttnn.CoreRange(_CORE, _CORE)])

def run_test(config_name: str, config) -> None:
    print(f"\n--- Testing config: {config_name} ---")
    device = ttnn.open_device(device_id=0)
    try:
        t = torch.zeros(OUTPUT_SIZE, dtype=torch.float32)
        output_t = ttnn.from_torch(t, dtype=ttnn.bfloat16, layout=ttnn.ROW_MAJOR_LAYOUT,
                                   device=device, memory_config=ttnn.DRAM_MEMORY_CONFIG)
        out_addr = output_t.buffer_address()
        print(f"  output buffer: {out_addr:#010x}")

        # generic_op requires ≥2 tensors (last = output). Use a small dummy input.
        dummy_t = ttnn.from_torch(torch.zeros(2, dtype=torch.float32), dtype=ttnn.bfloat16,
                                  layout=ttnn.ROW_MAJOR_LAYOUT, device=device,
                                  memory_config=ttnn.DRAM_MEMORY_CONFIG)

        defines = [("OUTPUT_DRAM_ADDR", hex(out_addr))]
        kernel_desc = ttnn.KernelDescriptor(
            kernel_source=KERNEL_PATH,
            source_type=ttnn.KernelDescriptor.SourceType.FILE_PATH,
            core_ranges=_CORE_RANGES,
            compile_time_args=[],
            named_compile_time_args=[],
            defines=defines,
            common_runtime_args=[],
            config=config,
        )
        program = ttnn.ProgramDescriptor(kernels=[kernel_desc], cbs=[], semaphores=[])

        t0 = time.time()
        ttnn.generic_op([dummy_t, output_t], program)  # dummy input + real output (last = output)
        elapsed = time.time() - t0
        print(f"  generic_op elapsed: {elapsed:.3f}s")

        # Correct decode: kernel writes raw bytes to bfloat16 DRAM tensor.
        # Convert back to bfloat16 then view as uint8 to recover original bytes.
        t_bf16 = ttnn.to_torch(output_t).to(torch.bfloat16)
        raw = bytes(t_bf16.view(torch.uint8).tolist())
        end = raw.find(0)
        text = raw[:end].decode("ascii", errors="replace") if end >= 0 else raw.decode("ascii", errors="replace")
        print(f"  output: {text!r}")
        if "HELLO" in text:
            print(f"  [PASS] Kernel executed and wrote output!")
        else:
            print(f"  [FAIL] No output (kernel did not execute or NoC write failed)")
    finally:
        ttnn.close_device(device)

if __name__ == "__main__":
    run_test("ReaderConfigDescriptor (NCRISC)", ttnn.ReaderConfigDescriptor())
    run_test("WriterConfigDescriptor (BRISC)",  ttnn.WriterConfigDescriptor())
