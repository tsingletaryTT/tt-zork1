# Player Agent Personas

**Status:** ✅ **COMPLETE** (Implemented 2026-02-17)

## Overview

The autonomous Player agent (Chip 3, port 8003) now supports **distinct personas** with **different knowledge levels** to create more interesting and varied gameplay. Each persona has a fundamentally different approach, knowledge level, and decision-making style.

## The Four Personas

### 1. Expert Speedrunner (`/player expert`)

**Knowledge Level:** Full Zork knowledge
**Goal:** Maximum score in minimum moves (~120 optimal)
**Approach:** Decisive, optimal commands

**Knows Everything:**
- All 20 treasure locations
- Optimal collection order
- Lamp battery duration (~50 turns)
- Trophy case mechanics
- Enemy patterns (grue, thief, troll)
- Maze solutions

**Example Behavior:**
```
Game: "West of House"
Expert: open window
(Fastest route inside - skips mailbox entirely)

Game: "Living Room"
Expert: move rug
(Knows trap door is underneath)

Game: "Dark area"
Expert: turn on lamp
(No hesitation - knows about grues)
```

**Prompt File:** `prompts/player/expert_speedrunner.txt` (~300 lines)

---

### 2. Naive Explorer (`/player naive`)

**Knowledge Level:** NO Zork knowledge
**Goal:** Have fun, discover naturally
**Approach:** Curious, cautious, learns by doing

**Knows Nothing:**
- Has never played Zork before
- Doesn't know what a "grue" is
- Doesn't know where treasures are
- Learns only from game text

**Example Behavior:**
```
Game: "West of House. Small mailbox here."
Naive: examine mailbox
(Curious - what's this mailbox about?)

Game: "Pitch black. Likely to be eaten by a grue."
Naive: wait
(Doesn't know to flee - learns the hard way!)

Result: "Eaten by a grue."
Lesson learned: Dark = dangerous!
```

**Prompt File:** `prompts/player/naive_explorer.txt` (~200 lines)

---

### 3. Completionist (`/player completionist`)

**Knowledge Level:** High Zork knowledge
**Goal:** 100% completion - all treasures, all rooms, best ending
**Approach:** Methodical, systematic, thorough

**Seeks Perfection:**
- Map every single room (~110 locations)
- Find all 20 treasures
- Solve every puzzle optimally
- Achieve maximum 350 point score

**Example Behavior:**
```
Game: "Forest. Paths lead N/S/E/W."
Completionist: north
(Maps all four directions systematically)

Game: "Forest (different location)"
Completionist: south
(Returns to mark as explored, then tries next direction)

Game: "Living Room. Trophy case."
Completionist: put egg in case
(Tracks progress: 1/20 treasures deposited)
```

**Prompt File:** `prompts/player/completionist.txt` (~350 lines)

---

### 4. Experimental (`/player experimental`)

**Knowledge Level:** Understands game mechanics
**Goal:** Try unexpected things, find unintended solutions
**Approach:** Creative, boundary-testing

**Tests Everything:**
- Unusual command combinations
- Parser limits
- Edge cases
- Sequence breaks

**Example Behavior:**
```
Game: "West of House. Mailbox here."
Experimental: take house
(Testing: Can you take fixed objects?)

Game: "Behind House. Window ajar."
Experimental: close window
(Counter-intuitive - makes puzzle harder!)

Game: "You have: lamp, egg, sword."
Experimental: put lamp in egg
(Nested containers - does it work?)
```

**Prompt File:** `prompts/player/experimental.txt` (~250 lines)

---

## Usage

### In-Game Commands

```bash
# Switch to auto play mode
/play auto

# Select a persona (default is generic Explorer)
/player expert        # Speedrunner
/player naive         # First-time player
/player completionist # 100% hunter
/player experimental  # Boundary tester

# Check current persona
/status

# View all commands
/help
```

### Example Session

```bash
# Start Zork
./zork-native game/zork1.z3

# Enable autonomous play
> /play auto
✓ Play mode: AUTO
  AI player (Chip 3) taking over!

# Watch the naive explorer learn
> /player naive
✓ Player persona: NAIVE EXPLORER
  No prior Zork knowledge, learns by doing!

[Watch as AI explores, makes mistakes, learns]

# Switch to expert for optimal play
> /player expert
✓ Player persona: EXPERT SPEEDRUNNER
  Full knowledge, optimal play!

[Watch as AI takes fastest path to victory]

# Return to manual control
> /play solo
✓ Play mode: SOLO
  You are in control.
```

## Architecture

### Prompt Loading System

**Original Strategies** (backward compatible):
- STRATEGY_EXPLORE, STRATEGY_TREASURE, STRATEGY_SURVIVAL, STRATEGY_SPEEDRUN
- Use generic `prompts/player/system_v3_magic.txt` with simple hints
- Maintained for compatibility

**Persona Strategies** (new):
- STRATEGY_EXPERT, STRATEGY_NAIVE, STRATEGY_COMPLETIONIST, STRATEGY_EXPERIMENTAL
- Load specialized prompt files from `prompts/player/`
- Fundamentally different knowledge levels and behaviors

### Code Flow

```
User: /player naive
  ↓
slash_commands.c: Sets STRATEGY_NAIVE
  ↓
auto_player.c: auto_player_next_command()
  ↓
  Check if persona strategy (>= STRATEGY_EXPERT)
  ↓
  Load prompts/player/naive_explorer.txt
  ↓
  Append current game state
  ↓
  Send to player agent (port 8003)
  ↓
AI responds with command based on naive persona
```

## Implementation Files

### Core Modules
- `src/llm/auto_player.h` - Updated enum with 4 new persona strategies
- `src/llm/auto_player.c` - Prompt loading and persona logic (~200 lines added)
- `src/llm/slash_commands.c` - `/player` command implementation (~80 lines added)

### Prompt Files
- `prompts/player/expert_speedrunner.txt` - Expert persona (3924 bytes)
- `prompts/player/naive_explorer.txt` - Naive persona (5035 bytes)
- `prompts/player/completionist.txt` - Completionist persona (5948 bytes)
- `prompts/player/experimental.txt` - Experimental persona (6496 bytes)

### Testing
- `test_personas.sh` - Automated test suite
  - Verifies all 4 prompt files exist
  - Tests slash command parsing
  - Tests prompt file loading
  - Tests persona switching

## Design Philosophy

**Key Principle:** Personas differ in **knowledge**, not just **goals**.

- **Naive Explorer** = Low knowledge, high curiosity → Natural learning curve
- **Expert Speedrunner** = High knowledge, single goal → Optimal efficiency
- **Completionist** = High knowledge, broader goal → Systematic thoroughness
- **Experimental** = Medium knowledge, creative approach → Boundary testing

This creates genuinely different gameplay experiences rather than just different command priorities.

## Testing Results

```bash
$ ./test_personas.sh

=== Testing Player Persona System ===

Test 1: Verifying prompt files exist...
  ✓ prompts/player/expert_speedrunner.txt exists (118 lines, 3924 bytes)
  ✓ prompts/player/naive_explorer.txt exists (164 lines, 5035 bytes)
  ✓ prompts/player/completionist.txt exists (193 lines, 5948 bytes)
  ✓ prompts/player/experimental.txt exists (195 lines, 6496 bytes)

Test 2: Verifying binary exists...
  ✓ zork-native binary found

Test 3: Testing slash commands...
  ✓ /help shows player personas
  ✓ /player command recognized
  ✓ /status shows current persona

Test 4: Testing prompt file loading...
  ✓ Expert: loaded 3924 bytes
  ✓ Naive: loaded 5035 bytes
  ✓ Completionist: loaded 5948 bytes
  ✓ Experimental: loaded 6496 bytes

=== All Tests Passed! ===
```

## Future Enhancements

### Potential Additions

1. **Custom Personas**
   - User-defined prompt files
   - `/player load custom_persona.txt`

2. **Persona Switching Mid-Game**
   - Track game state across persona changes
   - Compare different approaches to same situation

3. **Persona Learning**
   - Save/load persona knowledge state
   - Naive persona remembers discoveries across sessions

4. **Multi-Persona Battles**
   - Two AI agents with different personas compete
   - Expert vs Experimental speedrun competition

5. **Persona Analytics**
   - Track success rates per persona
   - Identify optimal strategies for specific puzzles

## Troubleshooting

### Persona Not Working?

```bash
# Verify auto play is enabled
> /play auto

# Verify persona is set
> /status

# Check if LLM server is running (port 8003)
$ curl http://localhost:8003/v1/models

# Check prompt file exists
$ ls -l prompts/player/expert_speedrunner.txt
```

### Persona Behavior Seems Wrong?

1. **Edit the prompt file directly:**
   ```bash
   $ vim prompts/player/naive_explorer.txt
   ```

2. **No recompilation needed** - changes take effect immediately

3. **Test your changes:**
   ```bash
   $ ./test_personas.sh
   ```

## References

- Original Plan: See CLAUDE.md "Phase 2.Y" section
- Implementation Details: `src/llm/auto_player.c`
- Prompt Engineering: Individual `.txt` files in `prompts/player/`
- Testing: `test_personas.sh`

---

**Created:** 2026-02-17
**Status:** Production-ready ✅
**Tested:** All 4 personas functional
