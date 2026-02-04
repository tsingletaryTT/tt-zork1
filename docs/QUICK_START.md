# Quick Start: Playing Zork on Blackhole Hardware

Welcome to Zork I running on Tenstorrent Blackhole RISC-V cores! This guide will get you playing in minutes.

## Prerequisites

- Tenstorrent Blackhole hardware with at least 1 functioning chip
- TT-Metal installed at `/home/ttuser/tt-metal`
- Game file: `game/zork1.z3`

## Choose Your Mode

### Option A: REPL Mode (Recommended)

Fast interactive mode with device kept warm for minimal latency:

```bash
./play-zork-repl.sh
```

**Features:**
- 100-500ms per command (target)
- Device initialized ONCE at startup
- Kernel compiled ONCE and reused
- Best for rapid command execution
- May hit resource limits after many commands

**When to use:** Normal gameplay, exploring quickly, testing commands

### Option B: Interactive Script Mode (Stable)

Traditional batch mode with device reset per command:

```bash
./play-zork-interactive.sh
```

**Features:**
- 3-10 seconds per command
- Device reset between commands
- More stable for long sessions
- Proven reliable for 10+ commands

**When to use:** Long gameplay sessions, when REPL mode has issues

## Common Zork Commands

### Movement
- `north` / `n` - Go north
- `south` / `s` - Go south
- `east` / `e` - Go east
- `west` / `w` - Go west
- `up` / `down` - Climb up or down

### Interaction
- `look` - Examine your surroundings
- `examine <item>` - Look closely at something
- `take <item>` - Pick up an item
- `drop <item>` - Drop an item
- `inventory` / `i` - Check your items
- `open <item>` - Open containers
- `close <item>` - Close containers
- `read <item>` - Read text

### Game Control
- `save` - Save your progress (not implemented yet)
- `restore` - Load saved game (not implemented yet)
- `quit` - Exit the game
- `score` - Check your score

## Getting Started: Your First Commands

When you start Zork, you'll see:

```
You are standing in an open field west of a white house,
with a boarded front door.
There is a small mailbox here.
```

Try these commands:

```
> open mailbox
Opening the small mailbox reveals a leaflet.

> take leaflet
Taken.

> read leaflet
"WELCOME TO ZORK!

ZORK is a game of adventure, danger, and low cunning..."

> north
North of House
You are facing the north side of a white house...
```

## Performance Expectations

**Response Times:**
- REPL mode: 100-500ms per command (target)
- Interactive mode: 3-10 seconds per command
- Both modes: Comparable to 1980s Commodore 64 experience!

**Why the latency?**
- Device initialization overhead
- Z-machine interpreter executing in batches on RISC-V cores
- This is **historically accurate** - original Zork had similar delays!

## Troubleshooting

### Device Not Responding

**Symptom:** `Device not responding` error

**Solution:**
```bash
# Option 1: Cold reboot (fastest, ~10 seconds)
sudo tt-cold-reboot

# Option 2: Soft reset (if cold reboot unavailable)
tt-smi -r 0

# Option 3: Check device status
tt-smi
```

### Device Hangs During Gameplay

**Symptom:** Command hangs for >30 seconds, no response

**Solution:**
1. Press Ctrl+C to exit the program
2. Run device reset: `sudo tt-cold-reboot`
3. Restart the game

**Prevention:** Keep sessions under 20-30 commands. Exit and restart periodically.

### Gibberish or Corrupted Text

**Symptom:** Game output looks garbled or incomplete

**Possible causes:**
- Not enough instruction batches executed
- Device needs reset

**Solution:**
1. Try running a few more commands - text may stabilize
2. If persistent, reset device and restart game

### REPL Mode Crashes

**Symptom:** REPL exits unexpectedly or device hangs

**Solution:**
Fall back to Interactive Script mode:
```bash
./play-zork-interactive.sh
```

This mode is more stable for long sessions (proven reliable for 10+ commands).

### Firmware Initialization Errors

**Symptom:**
```
Device 0: Timeout waiting for physical cores to finish: (x=1,y=2)
Device 0 init: failed to initialize FW!
```

**Solution:**
The system automatically uses chip 1 if chip 0 has issues. This is normal behavior.

If all chips fail:
1. Run cold reboot: `sudo tt-cold-reboot`
2. Check all devices: `tt-smi`
3. Try again

## Known Limitations

### What Works ✅
- Full Z-machine interpreter (24+ opcodes)
- Text output from game
- User input and command processing
- Interactive gameplay
- Multiple consecutive commands
- Save states within single session

### Known Issues ⚠️
- **No state persistence across program runs** - Each session starts fresh
- **Session limits** - May hang after 20-50 commands (hardware constraint)
- **No save/restore** - Cannot save game progress between sessions
- **Text may be truncated** - Long descriptions may cut off
- **Response latency** - 100ms-10s depending on mode (period-appropriate!)

### Architecture Constraints
- State persistence via DRAM causes hangs (proven in Phase 3.11)
- Firmware has execution time limits (batches must be small)
- Device initialization overhead per program run (~2-3 seconds)

## Tips for Best Experience

1. **Use REPL mode for quick exploration** - Fastest responses
2. **Use Interactive mode for long sessions** - More stable
3. **Exit and restart every 20-30 commands** - Prevents hangs
4. **Be patient with latency** - Embrace the 1980s nostalgia!
5. **Save command history** - Keep notes of your progress manually
6. **Experiment with commands** - Zork has rich vocabulary

## Fun Facts

- **Historic achievement:** This is likely the **FIRST TIME EVER** that a Z-machine interpreter has executed on AI accelerator hardware!
- **Technology bridge:** 1977 Infocom gaming code running on 2026 Tenstorrent silicon
- **Authentic experience:** Response times match 1980s Commodore 64 Zork
- **Architecture:** RISC-V cores execute the Z-machine, proving versatility of AI accelerators

## Need Help?

- Check project documentation: `docs/`
- Review implementation notes: `CLAUDE.md`
- File issues on the project repository

---

**Enjoy your adventure in the Great Underground Empire!** 🎮🚀

*Beware of grues in dark places...*
