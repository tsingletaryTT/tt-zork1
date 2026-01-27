# Python Orchestrator Approach

## Overview

Use Python TTNN API to orchestrate Z-machine execution on Blackhole RISC-V cores.

## Why Python?

1. **Automatic Program Caching**: Python API handles caching automatically
2. **Simpler Development**: Higher-level abstractions for buffers, kernels
3. **Proven Pattern**: Matches how production inference (vLLM, etc.) works
4. **Faster Iteration**: Python debugging vs C++ compile cycles

## Architecture

```python
device = ttnn.open_device(device_id=0)  # ONCE

# Load resources ONCE
game_buffer = create_dram_buffer(device, game_data)
state_buffer = create_dram_buffer(device, initial_state)

# Game loop - kernel cached after first run!
while not game_over:
    ttnn.run_kernel(device, "zork_interpreter", ...)  # 0.6s first, 0.001s after
    output = read_output_buffer(device)
    print(output)

ttnn.close_device(device)  # ONCE
```

## Implementation Plan

### Phase 1: Buffer Management ✅
- [x] Create Python skeleton
- [ ] Allocate DRAM buffers in Python
- [ ] Upload game data from Python
- [ ] Read output data to Python

### Phase 2: Kernel Integration
- [ ] Option A: PyBind11 wrapper for C++ kernel
- [ ] Option B: Use TTNN's CreateKernel from Python
- [ ] Test single batch execution

### Phase 3: Game Loop
- [ ] Implement persistent execution loop
- [ ] Verify program caching behavior
- [ ] Measure performance (should see 600x speedup after batch 1)

### Phase 4: Polish
- [ ] Add user input handling
- [ ] Display formatted game text
- [ ] Save/load state to disk

## Performance Expectations

Based on `program_cache.py` example:
- **First batch**: ~0.6s (compile + execute)
- **Subsequent batches**: ~0.001s (cache hit)
- **Speedup**: ~600x after first run

## References

- `/home/ttuser/tt-metal/ttnn/ttnn/examples/usage/program_cache.py`
- `/home/ttuser/tt-metal/ttnn/tutorials/basic_python/ttnn_mlp_inference_mnist.py`
