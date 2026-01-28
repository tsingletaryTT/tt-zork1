# Historical Development Documentation

This directory contains detailed development history and investigations from the Zork-on-Tenstorrent project. These documents track the journey from initial concept to working implementation.

## Purpose

These files are preserved for:
- Historical reference
- Understanding design decisions
- Learning from challenges and breakthroughs
- Detailed technical investigations
- Phase-by-phase development logs

## Organization

### Device Persistence Journey
- **BREAKTHROUGH_DEVICE_PERSISTENCE.md** - Major success with 5-batch execution
- **DEVICE_PERSISTENCE_SOLUTION.md** - Solution design documentation
- **SESSION_SCALING_SUCCESS.md** - Scaling from interpret(10) to interpret(100)
- **OPTIMIZATION_WORK.md** - Kernel optimization attempts
- **PINNED_MEMORY_DISCOVERY.md** - PinnedMemory API investigation
- **PINNED_MEMORY_STATUS.md** - Implementation status
- **PINNED_MEMORY_SUCCESS.md** - Host-side approach success
- **INVESTIGATION_RESULTS.md** - Technical investigations

### LLM Integration Journey
- **CONTEXT_FREE_MODE.md** - Discovery that context hurts small models
- **FASTPATH_COMMANDS.md** - Fast-path optimization design
- **LLM_USAGE.md** - Usage patterns and examples
- **MODEL_COMPARISON.md** - Testing different model sizes
- **OLLAMA_INTEGRATION.md** - Ollama setup and testing

### Phase Documentation
- **PHASE_1_2_COMPLETE.md** - Frotz integration completion
- **PHASE_1_3_READY.md** - RISC-V build system ready
- **PHASE_2_COMPLETE.md** - LLM integration completion
- **FINAL_STATUS.md** - Project status snapshot
- **INTEGRATION_STATUS.md** - Integration milestones
- **STATUS.md** - General status updates

### Technical Details
- **architecture.md** - Original architecture plans
- **frotz-integration.md** - Frotz integration details
- **V3_OPCODES.md** - Z-machine V3 opcode implementation tracking
- **RISCV_SETUP.md** - RISC-V toolchain setup

## Current Documentation

For up-to-date information, see:

- **[../README.md](../../README.md)** - Main project overview
- **[../BLACKHOLE_RISCV.md](../BLACKHOLE_RISCV.md)** - RISC-V execution guide
- **[../LLM_SUPPORT.md](../LLM_SUPPORT.md)** - LLM integration guide

## Key Insights Extracted

### Device Persistence Pattern ("Token Generation")
The breakthrough was realizing we should execute in small batches like LLM token generation:
- Open device ONCE
- Execute N small kernels
- Close device ONCE
- Result: 4× performance improvement

### Context-Free Translation
Small LLMs (0.5B-1.5B params) work better WITHOUT conversation context:
- Context-free: 100% accuracy with 1.5B model
- With context: 40% accuracy (model gets confused)
- Let the game handle disambiguation, not the LLM

### Fast-Path Optimization
Check for exact Zork commands before calling LLM:
- Exact commands: Instant (<1ms)
- Natural language: ~1-2 seconds
- Best of both worlds for players

### Hardware Accommodation
Work WITH hardware design, not against it:
- Blackhole designed for short kernels
- interpret(10-100) fits perfectly
- Long loops (150+) cause firmware timeouts
- Batching is the solution

## For Developers

When continuing this project, these documents provide:
- **Context** on why certain decisions were made
- **Debugging history** for similar issues
- **Performance data** from actual testing
- **Failed approaches** to avoid repeating
- **Successful patterns** to replicate

## Timeline

- **Jan 12, 2026:** Frotz integration complete, RISC-V build ready
- **Jan 12, 2026:** LLM integration with context-free mode
- **Jan 14, 2026:** Fast-path optimization, journey mapping
- **Jan 18-20, 2026:** Object table decoding, abbreviations working
- **Jan 20-22, 2026:** Z-machine interpreter on RISC-V, device persistence
- **Jan 22-23, 2026:** File-based state persistence attempts
- **Jan 28, 2026:** Device persistence breakthrough, scaling success

## License

These documents are part of the tt-zork1 project and follow the same license (Apache 2.0 for original code, GPL for Frotz integration).

---

*"History is not a burden on the memory but an illumination of the soul." - Lord Acton*

**These documents illuminate the path from concept to working Zork on AI accelerators.**
