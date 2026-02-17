# Knowledge Base - RAG for Text Adventures

This directory contains domain knowledge that makes the LLM agents true masters of text adventure games.

## Philosophy: RAG Without Embeddings

Instead of complex vector databases, we use **prompt engineering** to inject game-specific knowledge:

1. **Vocabulary files** - Game-specific commands, objects, locations
2. **Magic prompts** - Examples and patterns embedded in system prompts
3. **Domain expertise** - Deep understanding of IF (Interactive Fiction) conventions

This approach is:
- ✅ Simple to maintain (plain text files)
- ✅ Transparent (human-readable)
- ✅ Version controllable (git-friendly)
- ✅ Easy to extend (just add new files)

## Current Knowledge Base

### Zork I
- `zork1_vocab.txt` - Commands, objects, locations, survival tips
- Embedded in magic prompts (`../*/system_v*_magic.txt`)

### Universal IF Knowledge
Embedded in all magic prompts:
- Classic command syntax patterns
- Common puzzle types (keys/locks, light/dark, containers)
- Standard directional navigation
- Inventory management strategies

## Adding Knowledge for New Games

When adding support for a new Z-machine game:

### 1. Create Game Vocabulary File

```bash
cp zork1_vocab.txt hitchhikers_vocab.txt
# Edit to include game-specific knowledge
```

Include:
- Game-specific commands and aliases
- Important objects and their uses
- Key locations and connections
- Unique dangers or mechanics
- Survival tips and strategies

### 2. Create Game-Specific Prompt Variants

```bash
# For translator
cp ../translator/system_v5_magic.txt ../translator/system_v5_magic_hitchhikers.txt

# For artist
cp ../artist/system_v9_magic.txt ../artist/system_v9_magic_hitchhikers.txt

# etc.
```

Customize examples to match the new game's:
- Atmosphere (comedy vs horror vs sci-fi)
- Common objects (babel fish vs lamp)
- Typical locations (spaceship vs dungeon)

### 3. Update Config

```yaml
# config/llm_mode.yaml
mode: multi_agent

game: hitchhikers  # ← New field

endpoints:
  translator:
    prompt_file: prompts/translator/system_v5_magic_hitchhikers.txt
  # ...
```

## Knowledge Categories

### Commands & Syntax
What the translator needs to know:
- Canonical command forms
- Game-specific aliases
- Object name mappings

### Objects & Locations
What all agents need to know:
- Important items and their uses
- Key locations and atmosphere
- Danger zones and safe areas

### Puzzles & Strategies
What the player agent needs:
- Common puzzle patterns
- Optimal strategies
- Game-specific gotchas

### Narrative Style
What the DM needs:
- Game's unique voice (witty/dark/mysterious)
- Typical dramatic moments
- Atmospheric elements

## RAG Evolution Path

Current approach works well for 5-10 games. For larger scale:

### Phase 1: Current (✅)
- Hand-crafted knowledge per game
- Embedded in prompts
- Simple and effective

### Phase 2: Dynamic Context (Future)
- Load game vocab at runtime
- Inject into prompts dynamically
- Support 10-50 games

### Phase 3: True RAG (Future)
- Vector embeddings of game knowledge
- Semantic search for relevant facts
- Support 100+ games

## Example: Adding "The Lurking Horror"

1. **Create vocabulary**:
```
# lurking_horror_vocab.txt
## Setting: MIT campus + Lovecraftian horror

## Key Objects
- computer terminal - Access campus systems
- flashlight - Light source
- master key - Opens many doors
## Dangers
- rats - Hostile creatures
- darkness - Same as Zork (need light!)
- tentacles - Eldrich horror
```

2. **Adapt prompts**:
```
# translator/system_v5_magic_lurking_horror.txt
...
Input: "Use the computer"
Output: use terminal

Input: "Turn on my flashlight"
Output: turn on flashlight
...
```

3. **Update config**:
```yaml
game: lurking_horror
endpoints:
  translator:
    prompt_file: prompts/translator/system_v5_magic_lurking_horror.txt
```

## Knowledge Sources

### Where to find game knowledge:
1. **InvisiClues** - Official hint books
2. **Walkthrough FAQs** - GameFAQs, etc.
3. **IFDB reviews** - Interactive Fiction Database
4. **Source code** - For open-sourced games
5. **Play testing** - First-hand experience

### What to extract:
- All object names (exact spellings)
- All command variations
- Puzzle solutions (for player agent)
- Atmosphere keywords (for DM agent)
- Location connections (for all agents)

## Testing Knowledge Quality

Good knowledge base indicators:
- ✅ Translator rarely fails to parse valid commands
- ✅ Artist generates thematically appropriate scenes
- ✅ DM captures the game's unique voice
- ✅ Player makes strategic, informed decisions

Poor knowledge base indicators:
- ❌ Translator suggests non-existent commands
- ❌ Artist uses wrong aesthetic (scifi for fantasy)
- ❌ DM's tone mismatches game (serious for comedy)
- ❌ Player ignores game-specific dangers

## Contributing

When adding knowledge for a new game:
1. Create vocabulary file in this directory
2. Create game-specific prompt variants
3. Document in `GAMES_SUPPORTED.md`
4. Add example session in `examples/`

---

**Remember**: The goal isn't to spoil the game, but to make the agents competent players who understand the IF genre and specific game conventions. They should discover solutions through good strategy, not hardcoded walkthroughs.
