# Quick Start: Using Simplified Prompts + ASCII Art

## The Two Issues You Hit

### ❌ Issue 1: Auto Player Outputting Markdown
**What you saw:**
```
[AI Player → **Strategy: EXPLORE**,,You decide to try new directions...]
```

**Problem:** You used `/play auto` without setting a persona first, so it defaulted to the OLD verbose strategy.

**Fix:** Set persona FIRST, then enable auto-play:
```
> /player naive    ← Do this FIRST
> /play auto       ← Then this
```

### ❌ Issue 2: No ASCII Art
**Problem:** Port 8001 was running base model, not ASCII art specialist.

**Fix:** ASCII art server is starting now (~2 min). Will work once ready.

---

## Correct Usage

### Feature 1: Simplified AI Prompts

**SET PERSONA FIRST!** This is critical.

```bash
./zork-native game/zork1.z3

> /mode enhanced       # Enable LLM features
> /player naive        # ← SET PERSONA FIRST!
> /play auto           # Then enable auto-play
```

**Expected:** Clean commands like `examine mailbox`, `go north`
**NOT:** Markdown text with `**Strategy:**` headers

**To stop:** `> /play solo`

### Feature 2: ASCII Art

Art appears automatically when you move!

```
> /mode enhanced
> look              # See art for current location  
> go north          # Move → art appears!
```

---

## Quick Test

```bash
cd ~/code/tt-zork1
./zork-native game/zork1.z3

> /mode enhanced
> /player naive        # CRITICAL: Do this first!
> /play auto          # Now watch AI play
```

Expected: AI outputs clean commands, not markdown.

---

## Important: Slash vs No Slash

**Slash commands** (our custom):
- `/player naive` - Set persona
- `/play auto` - Auto-play
- `/help` - Show commands

**Game commands** (NO slash):
- `quit` - Exit (NOT /quit!)
- `save` - Save game
- `look` - Look around

---

## Troubleshooting

**"AI outputs markdown"**
→ You forgot `/player naive` before `/play auto`

**"No ASCII art"**
→ Wait 2 min for server startup
→ Check: `tt serve status | grep 8001`

**"'/quit' doesn't work"**
→ Use `quit` without slash

---

Check server status: `tt serve status`
Should show Llama-3.2-1B-ASCII-merged on port 8001
