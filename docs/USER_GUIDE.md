# Zork with LLM Agents - User Guide

Welcome to **LLM-Enhanced Zork**! This is the classic 1977 text adventure augmented with modern AI agents running on Tenstorrent hardware.

## Quick Start

```bash
# Build the game
./scripts/build_local.sh

# Play with all LLM features enabled
./play-zork-with-llm.sh

# Or play pure classic Zork
./zork-native game/zork1.z3
```

## Slash Commands - Control Everything In-Game

### Game Modes

**`/mode classic`** - Pure 1977 Zork
- No LLM features
- Exact original experience
- Type classic commands: "north", "take lamp", etc.

**`/mode enhanced`** - Full LLM augmentation (default)
- Natural language translation
- ASCII art for locations
- Journey postcards at game end
- All modern features enabled

### Play Modes

**`/play solo`** - You play (default)
- Type commands yourself
- Natural language translation available in enhanced mode
- Full control over decisions

**`/play auto`** - AI plays
- Autonomous player agent (Chip 3) takes over
- Watch the AI explore Zork
- Uses strategic decision-making
- Type `/play solo` to resume control

### Information Commands

**`/status`** - Show current settings
- Displays active game mode
- Shows active play mode
- Lists which LLM features are enabled

**`/help`** - Show available commands
- Lists all slash commands
- Explains what each mode does

## LLM Features Explained

### 1. Natural Language Translation (Chip 0)

**What it does**: Converts your natural language to Zork commands

**Examples**:
```
You type: "I want to go north"
LLM translates: "north"
Game receives: north

You type: "Can you pick up the brass lantern?"
LLM translates: "take lamp"
Game receives: take lamp
```

**When active**: Solo play + Enhanced mode

**To disable**: `/mode classic` or type exact Zork commands

### 2. ASCII Art Generation (Chip 1)

**What it does**: Creates themed emoji art for each location

**Example**:
```
Location: Forest

╔════════════════════════════════════╗
║  🎨 Forest                         ║
╠════════════════════════════════════╣
║  🌲🌳🌲🌳🌲                         ║
║  🌿🦌  🍃                           ║
║  🌳🌲  🌳🌲                         ║
║    🦉🍂🦉                           ║
╚════════════════════════════════════╝
```

**When active**: Enhanced mode (both play modes)

**Model**: Llama-3.2-1B-Instruct with magic prompts

### 3. Journey Postcards (Chip 2)

**What it does**: Creates dramatic descriptions of key moments

**Triggers**:
- First location (arrival)
- Finding treasures
- Death
- Victory
- Other significant events

**Example**:
```
📖 The white house looms before you, its boarded windows
like blind eyes watching nothing. A rusted mailbox creaks
in the wind, the only sound in this abandoned place. Your
adventure begins here, at the threshold between the known
world and the Great Underground Empire.
```

**Display**: Shown alongside journey map at game end

**When active**: Enhanced mode

**Model**: Qwen3-0.6B with magic prompts

### 4. Autonomous Player (Chip 3)

**What it does**: AI plays Zork using strategic decision-making

**Strategies**:
- **Explore**: Systematic room exploration, map building
- **Treasure**: Focus on finding and collecting valuables
- **Survival**: Conservative play, avoid danger
- **Speedrun**: Optimal path to victory (future)

**When active**: Auto play mode + Enhanced mode

**Model**: Qwen3-0.6B with magic prompts

**To watch**: `/play auto` then sit back!

## Typical Play Sessions

### Casual Player (Natural Language)

```bash
./play-zork-with-llm.sh

> /status
Game Mode: ENHANCED
Play Mode: SOLO

> I want to explore the house
[LLM → enter house]
You are in the living room...

> What's in the trophy case?
[LLM → examine trophy case]
The trophy case is empty.

> Let me go check out the kitchen
[LLM → go to kitchen]
```

### Classic Zork Purist

```bash
./zork-native game/zork1.z3

> /mode classic
✓ Pure Zork mode enabled!

> open mailbox
Opening the small mailbox reveals a leaflet.

> take leaflet
Taken.

> read leaflet
"WELCOME TO ZORK!..."
```

### AI Demonstration

```bash
./play-zork-with-llm.sh

> /play auto
✓ AI player taking over!

[AI]: examine mailbox
The mailbox contains a leaflet.

[AI]: take leaflet
Taken.

[AI]: read leaflet
"WELCOME TO ZORK!..."

[AI]: north
You are in a forest...

> /play solo
✓ You are back in control!
```

### Best of Both Worlds

```bash
./play-zork-with-llm.sh

> /status
Mode: ENHANCED / SOLO

# Start with natural language
> I want to open the mailbox
[LLM → open mailbox]
Done.

# Switch to classic commands for precision
> take leaflet
Taken.

> read leaflet
"WELCOME TO ZORK!..."

# Back to natural language
> Can you show me what I'm carrying?
[LLM → inventory]
You are carrying: a leaflet

# Watch AI play for a bit
> /play auto
[AI explores...]

# Take back control
> /play solo
```

## Magic Prompts - What Makes It Smart

Each LLM agent has **domain expertise** embedded in its prompts:

**Translator** knows:
- Classic adventure game syntax
- Zork-specific objects (lamp, sword, mailbox)
- Common abbreviations (N/S/E/W, I, X)
- 20+ translation patterns

**Artist** knows:
- 60+ themed emojis (🏠🌲⛰️💎💀)
- Roguelike/Nethack aesthetic
- Classic adventure location types
- 6 diverse location examples

**DM** knows:
- Infocom narrative style
- Zork atmosphere (mystery, danger, wonder)
- Text adventure pacing
- 6 cinematic postcard examples

**Player** knows:
- Classic adventure wisdom (EXAMINE EVERYTHING!)
- Common puzzle patterns
- Zork dangers (grues, thieves, trolls)
- Survival strategies

See `docs/llm/MAGIC_PROMPTS.md` for technical details.

## Journey Map

At the end of your game (death or victory), you'll see:

1. **ASCII Map** - Visual representation of rooms visited
2. **Journey Postcards** - Dramatic moments from your adventure
3. **Statistics** - Rooms visited, steps taken

Example:
```
╔════════════════════════════════════════════════╗
║        YOUR JOURNEY THROUGH ZORK              ║
╠════════════════════════════════════════════════╣
║                                                ║
║   +------------+  +------------+               ║
║   |  W.House   |> |   Forest   |               ║
║   +------------+  +------------+               ║
║                                                ║
║   +------------+                               ║
║   | Clearing   |                               ║
║   +------------+                               ║
╚════════════════════════════════════════════════╝

📬 POSTCARDS FROM YOUR JOURNEY

[Dramatic moment 1...]
[Dramatic moment 2...]
```

## Troubleshooting

### "LLM features not working"

Check servers are running:
```bash
tt serve status
# Should show servers on ports 8000, 8001, 8002, 8003
```

### "Commands not translating"

1. Check mode: `/status`
2. Should show: ENHANCED + SOLO
3. If CLASSIC: `/mode enhanced`
4. If AUTO: `/play solo`

### "Want pure Zork"

```bash
> /mode classic
# Now you're playing original Zork!
```

### "AI player stuck"

```bash
> /play solo
# Take back manual control
```

## Advanced: Environment Variables

```bash
# Disable specific features
export ZORK_LLM_ENABLED=0        # Disable translator
export ZORK_ARTIST_ENABLED=0     # Disable artist
export ZORK_DM_ENABLED=0         # Disable DM
export ZORK_PLAYER_ENABLED=0     # Disable auto-player

# Alternative: Just use /mode classic in-game!
```

## Performance

**Translation latency**: ~1-2 seconds per command
**Art generation**: ~2-3 seconds per location
**Postcard creation**: ~1-2 seconds per moment
**Auto-play speed**: ~2-3 seconds per decision

Commands matching exact Zork syntax (like "north", "take lamp") skip LLM translation for instant response (fast-path optimization).

## Hardware Details

**Current Setup** (2× P300C Blackhole chips):
- Chip 0, Port 8000: Translator (Qwen3-0.6B)
- Chip 1, Port 8001: Artist (Llama-3.2-1B)
- Chip 0, Port 8002: DM (Qwen3-0.6B)
- Chip 0, Port 8003: Player (Qwen3-0.6B)

Multiple servers can run on one chip with different ports.

## What's Next?

### Try Other Games

The system works with any Z-machine game:
- Hitchhiker's Guide to the Galaxy
- The Lurking Horror
- Planetfall
- Leather Goddesses of Phobos

Just provide the `.z3` file! (Prompts are Zork-optimized but work generally)

### Improve Prompts

Edit files in `prompts/` to tune agent behavior:
- `translator/system_v5_magic.txt` - Translation rules
- `artist/system_v9_magic.txt` - Art generation
- `dm/system_v5_magic.txt` - Narrative style
- `player/system_v3_magic.txt` - AI strategy

See `prompts/knowledge/README.md` for multi-game support.

## Have Fun!

You're playing a piece of computing history augmented with cutting-edge AI. Enjoy exploring the Great Underground Empire with your LLM companions!

**Remember**: Beware of grues! 🦇

---

*Questions? Check `docs/` directory for technical documentation.*
