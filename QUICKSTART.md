# Quick Start Guide

## 1. Verify Setup

```bash
# Test all LLM servers
$ ./test_llm_integration.sh

# Should show all green checkmarks (✓)
```

## 2. Start Playing

```bash
# Start Zork
$ ./zork-native game/zork1.z3
```

## 3. Try Each Feature

### A. Natural Language Translation

```bash
# Enable enhanced mode (if not already)
> /mode enhanced

# Try natural language
> I want to open the mailbox
[Translating: "I want to open the mailbox" → "open mailbox"]
Opening the small mailbox reveals a leaflet.

> Please pick up the leaflet and read it
[Translating: "Please pick up the leaflet and read it" → "take leaflet. read leaflet"]
Taken.
"WELCOME TO ZORK! ..."
```

### B. Slash Commands

```bash
# View help
> /help

# Check status
> /status

# Switch modes
> /mode classic   # Pure Zork (no LLM)
> /mode enhanced  # All LLM features
```

### C. Auto-Play Mode (Watch AI Play)

```bash
# Enable auto-play
> /play auto

# Select a persona
> /player naive

# Now just press Enter repeatedly to watch the AI play
>
[AI Player → examine mailbox]
Opening the small mailbox reveals a leaflet.

>
[AI Player → take leaflet]
Taken.

>
[AI Player → read leaflet]
"WELCOME TO ZORK! ..."
```

### D. Try Different Personas

```bash
# Naive explorer (learns as it goes)
> /player naive
[AI Player → what's this mailbox?]

# Expert speedrunner (knows everything)
> /player expert
[AI Player → open window]

# Completionist (100% completion)
> /player completionist
[AI Player → examine mailbox]
[AI Player → examine house]
[AI Player → north]

# Experimental (tests boundaries)
> /player experimental
[AI Player → take house]
```

### E. Return to Manual Control

```bash
> /play solo
✓ Play mode: SOLO
  You are in control.

> look
West of House
You are standing in an open field...
```

## 4. Features to Observe

### Natural Language Translation

Type commands naturally:
- "I want to go north" → "north"
- "open the mailbox" → "open mailbox"
- "pick up the lamp" → "take lamp"

### ASCII Art (Automatic)

After location descriptions, watch for ASCII art:
```
West of House
You are standing in an open field...

    _____________
   |   _____     |
   |  |     |    |
   |  | [_] |    |
   |__|_____|____|
     |||     |||
     |||     |||
    House
```
*(Takes 2-3 seconds to generate)*

### Journey Map (On Death/Victory)

When you die or win, a 2D map shows your journey:
```
╔════════════════════════════════════════════════╗
║        YOUR JOURNEY THROUGH ZORK              ║
╠════════════════════════════════════════════════╣
║                                                ║
║   +------------+  +------------+               ║
║   |  W.House   |> |   Forest   |               ║
║   +------------+  +------------+               ║
╠════════════════════════════════════════════════╣
║ Rooms: 4  Connections: 3  Size: 2x2           ║
╚════════════════════════════════════════════════╝
```

## 5. Test All Personas

### Naive Explorer (First-Timer)
```bash
> /player naive
# Watch it discover things:
> [AI → examine mailbox]    # Curious about objects
> [AI → read leaflet]        # Learning from text
> [AI → north]               # Exploring naturally
> [AI → wait]                # Gets eaten by grue! (learns the hard way)
```

### Expert Speedrunner (Pro Player)
```bash
> /player expert
# Watch optimal play:
> [AI → open window]         # Fastest route inside
> [AI → get lamp]            # Knows it's critical
> [AI → turn on lamp]        # Prepares for darkness
```

### Completionist (100% Hunter)
```bash
> /player completionist
# Watch systematic exploration:
> [AI → examine mailbox]
> [AI → examine house]
> [AI → north]
> [AI → examine forest]
# Maps every location methodically
```

### Experimental (Boundary Tester)
```bash
> /player experimental
# Watch creative attempts:
> [AI → take house]          # Tests impossible actions
> [AI → put lamp in egg]     # Tests nested containers
> [AI → smell grue]          # Tests unusual verbs
```

## 6. Quick Commands Reference

### Slash Commands
```
/help              - Show all commands
/status            - Show current settings
/mode classic      - Disable LLM features
/mode enhanced     - Enable all LLM features
/play solo         - Manual control (you play)
/play auto         - AI control (watch AI play)
/player naive      - Naive explorer persona
/player expert     - Expert speedrunner persona
/player completionist - Completionist persona
/player experimental  - Experimental persona
```

### Game Commands (Examples)
```
Exact commands (instant, no LLM):
  north, south, east, west, n, s, e, w
  look, inventory, take lamp, open mailbox

Natural language (uses LLM translation):
  "I want to go north" → north
  "Please open the mailbox" → open mailbox
  "Can you pick up the lamp?" → take lamp
```

## 7. Troubleshooting

If something isn't working:

```bash
# 1. Check all servers running
$ ./test_llm_integration.sh

# 2. Verify you're in enhanced mode
> /status
# Should show: Game Mode: ENHANCED

# 3. Rebuild if needed
$ ./scripts/build_local.sh

# 4. See detailed troubleshooting
$ cat TROUBLESHOOTING.md
```

## 8. Stop Servers When Done

```bash
# Stop all LLM servers
$ ./scripts/stop-four-llms.sh
```

---

## Expected Experience

### First Launch
```bash
$ ./zork-native game/zork1.z3

ZORK I: The Great Underground Empire
Infocom interactive fiction - a fantasy story
© Infocom, Inc. All rights reserved.
ZORK is a registered trademark of Infocom, Inc.
Release 88 / Serial number 840726

West of House
You are standing in an open field west of a white house, with a
boarded front door. There is a small mailbox here.

>
```

### Using Natural Language
```bash
> I want to check what's inside the mailbox
[Translating: "I want to check what's inside the mailbox" → "open mailbox"]
Opening the small mailbox reveals a leaflet.

>
```

### Watching AI Play
```bash
> /play auto
✓ Play mode: AUTO

> /player naive
✓ Player persona: NAIVE EXPLORER

>
[AI Player → examine mailbox]
The small mailbox is open. In the mailbox is a leaflet.

>
[AI Player → take leaflet]
Taken.

>
[AI Player → read leaflet]
"WELCOME TO ZORK! ..."
```

---

**Enjoy exploring Zork with AI-powered features!** 🎮🤖✨

For more details, see:
- `docs/PLAYER_PERSONAS.md` - Persona system documentation
- `docs/USER_GUIDE.md` - Complete user guide
- `TROUBLESHOOTING.md` - Detailed troubleshooting
- `test_llm_integration.sh` - Automated testing
