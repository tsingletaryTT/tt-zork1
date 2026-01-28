# Quick Start Guide

**Get Zork running in under 2 minutes!**

## Native Build (Play on your computer)

### With LLM Natural Language

```bash
# Start Ollama and pull model (first time only)
ollama pull qwen2.5:1.5b

# Play with natural language
./run-zork-llm.sh
```

**Try commands like:**
- "I want to open the mailbox"
- "Please take the leaflet"
- "Can you go north?"

### Classic Mode (No LLM)

```bash
# Build
./scripts/build_local.sh

# Play
./zork-native game/zork1.z3
```

**Use standard Zork commands:**
- north, south, east, west
- take lamp
- open mailbox
- inventory

## Hardware Build (Blackhole RISC-V)

**⚠️ Note:** Device must be healthy. If stuck, run: `tt-smi -r 0`

### One-Command Launcher

```bash
# Build (first time only)
cd build-host
cmake --build . --target test_zork_optimized
cd ..

# Play on hardware!
./play-zork-hardware.sh
```

**Options:**
```bash
./play-zork-hardware.sh 5   # 5 batches (default, safe)
./play-zork-hardware.sh 10  # More batches (may hang!)
```

**If device hangs:**
```bash
sudo tt-cold-reboot  # Quick driver reload (~10s)
# OR
tt-smi -r 0          # Full reset (~15s)
```

## Current Status

### Native Build
- ✅ **Fully playable**
- ✅ LLM translation (100% accuracy with qwen2.5:1.5b)
- ✅ Fast-path optimization (instant exact commands)
- ✅ Journey mapping (ASCII art of your path)
- ✅ Complete game experience

### Hardware Build
- ✅ **Execution proven** on Blackhole RISC-V
- ✅ Device persistence working
- ✅ 500 instructions per run (5 batches × 100)
- ✅ ~7 seconds execution time
- ✅ Real Zork text output
- ⚠️ Text-only (no input handling yet)
- ⚠️ Limited to 5 batches (hardware constraint)

## What Each Launcher Does

### `run-zork-llm.sh`
**Native with LLM:**
- Starts Frotz Z-machine interpreter
- Enables LLM natural language translation
- Full gameplay with conversational commands
- Shows journey map on death/victory

### `./zork-native`
**Native classic:**
- Pure Frotz interpreter
- Traditional two-word commands
- No LLM overhead
- Fast and simple

### `play-zork-hardware.sh`
**Blackhole RISC-V:**
- Runs Z-machine on AI accelerator cores
- Device persistence (3.2× faster than single-shot)
- Displays Zork opening text
- Technical demonstration of classic gaming on AI hardware

## Performance Comparison

| Mode | Latency | Features | Status |
|------|---------|----------|--------|
| Native classic | Instant | Full game | ✅ Production |
| Native + LLM | 1-2s per command | Natural language | ✅ Production |
| Blackhole RISC-V | 7s per 500 instructions | Hardware proof | ✅ Working |

## Troubleshooting

### "Device failed to initialize"
```bash
tt-smi -r 0  # Reset device
sleep 5      # Wait for init
./play-zork-hardware.sh
```

### "Program timed out"
Device hung. Recovery:
```bash
pkill -f test_zork  # Kill stuck process
tt-smi -r 0         # Reset device
```

### "LLM translation failed"
```bash
# Check Ollama is running
ollama list

# Restart if needed
killall ollama
ollama serve &
sleep 2

# Run again
./run-zork-llm.sh
```

### "Build failed"
```bash
# Native build
./scripts/build_local.sh clean
./scripts/build_local.sh

# Hardware build
cd build-host
rm -rf *
cmake ..
cmake --build . --parallel
cd ..
```

## Next Steps

After getting Zork running:

1. **Read the docs:**
   - [TENSTORRENT_EXPLAINED.md](docs/TENSTORRENT_EXPLAINED.md) - Beginner's guide
   - [BLACKHOLE_RISCV.md](docs/BLACKHOLE_RISCV.md) - Hardware details
   - [LLM_SUPPORT.md](docs/LLM_SUPPORT.md) - Natural language guide

2. **Experiment:**
   - Try different LLM models
   - Test hardware batch limits
   - Modify prompts for better translation

3. **Contribute:**
   - Add more opcodes to RISC-V interpreter
   - Implement input handling on hardware
   - Optimize batch execution

## Quick Reference

**One-line commands:**

```bash
# Native LLM: ./run-zork-llm.sh
# Native classic: ./zork-native game/zork1.z3
# Hardware: ./play-zork-hardware.sh
# Reset device: tt-smi -r 0
# Cold reboot: sudo tt-cold-reboot
```

---

**Welcome to Zork on Tenstorrent!** 🎮🚀

*"You are standing in an open field west of a white house, with a boarded front door."*

**> open mailbox**
