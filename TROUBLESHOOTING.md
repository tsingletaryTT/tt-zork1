# Troubleshooting Guide

## Quick Fixes for Common Issues

### Issue 1: "Only accepts exact commands" / "Translation not working"

**Symptom:** You type natural language like "I want to open the mailbox" but it doesn't translate.

**Cause:** Game is in CLASSIC mode or translator is disabled.

**Fix:**
```bash
# In game, type:
> /mode enhanced
> /status

# Should show:
#   Game Mode: ENHANCED
#   ✓ Natural language translation (Chip 0)
```

**If still not working:**
```bash
# Check if translator server is running:
$ curl -s http://localhost:8000/v1/models | jq -r '.data[0].id'
# Should output: Qwen3-0.6B

# If not running, start it:
$ tt serve start Qwen3-0.6B --device-id 0 --port 8000
```

---

### Issue 2: "No ASCII art appearing"

**Symptom:** Location descriptions don't show ASCII art.

**Cause:** Artist server (Chip 1) not running or not configured.

**Fix:**
```bash
# Check if artist server is running:
$ curl -s http://localhost:8001/v1/models | jq -r '.data[0].id'
# Should output: Llama-3.2-1B-Instruct

# If not running, start it:
$ tt serve start Llama-3.2-1B-Instruct --device-id 1 --port 8001

# In game, verify it's enabled:
> /status
# Should show: ✓ ASCII art generation (Chip 1)
```

**Note:** ASCII art generation happens in background. May take 2-3 seconds to appear after location description.

---

### Issue 3: "Slash commands being echoed to game" / "Commands don't work"

**Symptom:** When you type `/help`, the game responds with "I beg your pardon?" or "I don't understand that."

**Cause:** Slash commands should be intercepted BEFORE being sent to Z-machine.

**Fix:**
This was fixed in commit 83b55b8. If you're still seeing this:

```bash
# Rebuild:
$ ./scripts/build_local.sh

# Verify slash commands work:
$ ./zork-native game/zork1.z3
> /help
# Should show slash command help, NOT game error

# If you see game error, the slash command system isn't working.
# Check that you have the latest code:
$ git log --oneline | head -3
```

---

### Issue 4: "Auto mode doesn't start" / "AI not playing"

**Symptom:** You type `/play auto` but the game still waits for your input instead of AI playing.

**Cause:** Auto-player was not integrated into input loop (fixed in commit 83b55b8).

**Fix:**
```bash
# Rebuild with latest code:
$ git pull
$ ./scripts/build_local.sh

# Start game and test:
$ ./zork-native game/zork1.z3

# Enable auto-play:
> /play auto
✓ Play mode: AUTO
  AI player (Chip 3) taking over!

# Select a persona:
> /player naive
✓ Player persona: NAIVE EXPLORER

# Now press Enter (empty line)
# You should see:
[AI Player → examine mailbox]
```

**If AI still doesn't play:**
```bash
# Check Player agent server:
$ curl -s http://localhost:8003/v1/models | jq -r '.data[0].id'
# Should output: Qwen3-0.6B

# If not running:
$ tt serve start Qwen3-0.6B --device-id 3 --port 8003

# Verify persona files exist:
$ ls -l prompts/player/*.txt
# Should show:
#   expert_speedrunner.txt
#   naive_explorer.txt
#   completionist.txt
#   experimental.txt
```

---

## Comprehensive Test

Run the automated test to check all systems:

```bash
$ ./test_llm_integration.sh

# Should show all green checkmarks (✓) for:
# - All 4 LLM servers running
# - Config file exists
# - Translator working
# - Artist working
# - Player agent working
# - All prompt files present
```

---

## Start All LLM Servers

If some servers aren't running:

```bash
# Option 1: Use the provided script
$ ./scripts/start-four-llms.sh

# Option 2: Start individually
$ tt serve start Qwen3-0.6B --device-id 0 --port 8000 --detach           # Translator
$ tt serve start Llama-3.2-1B-Instruct --device-id 1 --port 8001 --detach  # Artist
$ tt serve start Qwen3-0.6B --device-id 2 --port 8002 --detach           # DM/Postcard
$ tt serve start Qwen3-0.6B --device-id 3 --port 8003 --detach           # Player

# Verify all running:
$ ps aux | grep "tt serve" | grep -v grep
```

---

## Stop All LLM Servers

```bash
# Option 1: Use the provided script
$ ./scripts/stop-four-llms.sh

# Option 2: Stop individually
$ tt serve stop --port 8000
$ tt serve stop --port 8001
$ tt serve stop --port 8002
$ tt serve stop --port 8003
```

---

## Feature-Specific Tests

### Test Natural Language Translation

```bash
$ ./zork-native game/zork1.z3
> /mode enhanced
> I want to open the mailbox
# Should translate to: open mailbox
# Game response: Opening the small mailbox reveals a leaflet.
```

### Test ASCII Art

```bash
$ ./zork-native game/zork1.z3
# Wait for opening text
# After "West of House" location description
# ASCII art of a house should appear (takes 2-3 seconds)
```

### Test Slash Commands

```bash
$ ./zork-native game/zork1.z3
> /help          # Show all commands
> /status        # Show current settings
> /mode classic  # Disable LLM features
> /mode enhanced # Re-enable LLM features
> /play solo     # Manual control
> /play auto     # AI control
```

### Test Auto-Play with Different Personas

```bash
$ ./zork-native game/zork1.z3
> /play auto
> /player naive
# Watch naive explorer learn the game

> /player expert
# Watch expert speedrunner optimize

> /player completionist
# Watch systematic 100% completion

> /player experimental
# Watch creative boundary testing

> /play solo
# Return to manual control
```

---

## Configuration Files

### Check Configuration

```bash
$ cat config/llm_mode.yaml
# Should show:
#   mode: multi_agent
#   endpoints:
#     translator: url: http://localhost:8000/v1/chat/completions
#     artist: url: http://localhost:8001/v1/chat/completions
#     dm: url: http://localhost:8002/v1/chat/completions
#     player: url: http://localhost:8003/v1/chat/completions
```

### Check Prompt Files

```bash
# Translator prompts
$ ls prompts/translator/
system_v5_magic.txt

# Artist prompts
$ ls prompts/artist/
system_v9_magic.txt

# DM prompts
$ ls prompts/dm/
system_v5_magic.txt

# Player prompts
$ ls prompts/player/
completionist.txt
experimental.txt
expert_speedrunner.txt
naive_explorer.txt
system_v3_magic.txt
```

---

## Still Having Issues?

1. **Check git log for latest commits:**
   ```bash
   $ git log --oneline | head -5
   # Should include:
   #   83b55b8 fix: Integrate auto-player into game loop
   #   f84d6a0 feat: Add Player Agent Personas
   ```

2. **Rebuild from scratch:**
   ```bash
   $ ./scripts/build_local.sh clean
   $ ./scripts/build_local.sh
   ```

3. **Verify all dependencies:**
   ```bash
   # Check libcurl
   $ curl --version

   # Check jq (for testing)
   $ jq --version

   # Check tt-smi
   $ tt-smi -s
   ```

4. **Check device health:**
   ```bash
   $ tt-smi -s
   # All devices should show "Ready"
   ```

5. **Review logs:**
   ```bash
   # If auto-play isn't working, check for error messages:
   $ ./zork-native game/zork1.z3 2>&1 | tee game.log
   # Then check game.log for errors
   ```

---

## Expected Behavior Summary

### ✅ Working Features

- ✅ Natural language translation (Chip 0)
- ✅ ASCII art generation (Chip 1)
- ✅ Journey postcards (Chip 2)
- ✅ Autonomous player (Chip 3)
- ✅ Slash commands (/help, /mode, /play, /player, /status)
- ✅ 4 player personas (expert, naive, completionist, experimental)
- ✅ Fast-path optimization (exact commands skip LLM)
- ✅ Journey map on death/victory

### 🚧 Known Limitations

- Auto-play currently uses placeholder game state (not full output_capture)
- AI may generate suboptimal commands without full context
- ASCII art generation may be slow (~2-3 seconds)
- Small models (0.5B) may need retries for complex translations

### 🎯 Performance Expectations

- Natural language translation: ~0.5-1.5 seconds
- ASCII art generation: ~2-3 seconds
- Auto-play command generation: ~0.5-1 second
- Exact commands (fast-path): ~0.001 seconds (instant)

---

**Last Updated:** 2026-02-17 (commit 83b55b8)
