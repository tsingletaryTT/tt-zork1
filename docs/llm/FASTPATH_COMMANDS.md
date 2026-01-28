# Fast-Path Command Reference

This document lists commands that bypass LLM translation for instant response.

## How It Works

When you type a command, the system checks if it matches known Zork syntax:
- **Fast-Path (Instant)**: Recognized Zork commands bypass the LLM → ~0.001s response
- **LLM Translation (Slower)**: Natural language goes through LLM → ~1s response

Both paths:
- ✅ Work correctly with the game
- ✅ Are tracked for journey mapping
- ✅ Support all Zork features

## Fast-Path Commands

### Single-Word Directions
These are **instant** - no LLM needed:
```
north, south, east, west, up, down, in, out
n, s, e, w, u, d
northeast, northwest, southeast, southwest
ne, nw, se, sw
```

### Single-Word Verbs
These are **instant** - no LLM needed:
```
look, l
inventory, i
wait, z
save, restore, restart
quit, q
score, help, version
verbose, brief
diagnose
again, g
```

### Two-Word Commands (Verb + Object)
These are **instant** if they match the pattern:
```
take lamp          ✅ Fast-path
get sword          ✅ Fast-path
drop lantern       ✅ Fast-path
open mailbox       ✅ Fast-path
close door         ✅ Fast-path
read book          ✅ Fast-path
examine trophy     ✅ Fast-path
x statue           ✅ Fast-path
push button        ✅ Fast-path
pull lever         ✅ Fast-path
turn dial          ✅ Fast-path
attack troll       ✅ Fast-path
eat sandwich       ✅ Fast-path
drink water        ✅ Fast-path
climb stairs       ✅ Fast-path
enter house        ✅ Fast-path
```

### Recognized Verbs (for Multi-Word Commands)
Any command starting with these verbs is **instant**:
```
take, get, drop, put
open, close, lock, unlock
read, examine, x, look
push, pull, turn, move
attack, kill, eat, drink
wear, remove, climb, enter
exit, light, extinguish
```

## LLM Translation Examples

These natural language inputs will use the LLM (slower but flexible):

```
"please go north"                    → LLM translates to "north"
"I want to take the lamp"            → LLM translates to "take lamp"
"can you open the mailbox for me?"   → LLM translates to "open mailbox"
"let's head east"                    → LLM translates to "east"
"pick up the sword"                  → LLM translates to "take sword"
"what do I have?"                    → LLM translates to "inventory"
"where am I?"                        → LLM translates to "look"
```

## Performance Comparison

| Input Type | Processing Time | Goes Through LLM? |
|------------|----------------|-------------------|
| `north` | ~0.001s | ❌ No (fast-path) |
| `take lamp` | ~0.001s | ❌ No (fast-path) |
| `open mailbox` | ~0.001s | ❌ No (fast-path) |
| `please go north` | ~1.0s | ✅ Yes (translation needed) |
| `I want to take the lamp` | ~1.0s | ✅ Yes (translation needed) |
| `can you open the mailbox?` | ~1.0s | ✅ Yes (translation needed) |

## Tips for Fastest Gameplay

1. **Learn Zork syntax** - Commands like "north", "take lamp", "open mailbox" are instant
2. **Use abbreviations** - "n", "s", "e", "w", "i", "l" are all fast-path
3. **Skip articles** - "take lamp" (fast) vs "take the lamp" (fast too!)
4. **Use natural language when stuck** - The LLM is there when you need it!

## Technical Notes

- Fast-path checking is **syntactic**, not semantic
  - "open mailbox" → fast-path (syntactically valid)
  - Game will tell you if mailbox isn't available (semantic validation)
- Journey tracking works for both paths
- Commands are case-insensitive
- Fast-path adds ~30 lines of code, negligible performance impact

## Testing

Run the fast-path test:
```bash
./test-fastpath.sh
```

This verifies:
- Exact commands bypass LLM
- Natural language uses LLM
- Both paths work correctly
- Performance difference is measurable
