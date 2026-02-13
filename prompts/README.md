# Prompts for 4-Chip Zork Architecture

This directory contains optimized prompts for each of the 4 specialized LLMs.

## Directory Structure

```
prompts/
├── config.yaml                    # Configuration and experiment settings
├── translator/                    # Command translation prompts
│   ├── system_v1_minimal.txt     # Ultra-concise (fastest)
│   ├── system_v2_detailed.txt    # More examples (more accurate)
│   └── system_v3_fewshot.txt     # Pattern-based (recommended) ⭐
├── artist/                        # ASCII art generation prompts
│   ├── system_v1_simple.txt      # Clean minimal art
│   └── system_v2_atmospheric.txt # Detailed moody art (recommended) ⭐
├── dm/                            # Dungeon master enhancement prompts
│   ├── system_v1_subtle.txt      # Light atmospheric touches (recommended) ⭐
│   └── system_v2_dramatic.txt    # Rich narrative enhancements
└── player/                        # AI player strategy prompts
    ├── system_v1_explorer.txt    # Basic exploration strategy
    └── system_v2_strategic.txt   # Advanced thinking (recommended) ⭐
```

## Quick Start

```bash
# 1. Test all prompt variants
./scripts/test-prompt-variants.sh

# 2. Compare variants side-by-side
./scripts/compare-prompts.sh translator "open the mailbox"

# 3. Tune interactively
./scripts/tune-prompt-interactive.sh
```

## Recommended Configurations

- **Speed**: minimal translator, simple artist, DM off
- **Balanced**: fewshot translator, atmospheric artist, subtle DM ⭐
- **Immersive**: fewshot translator, atmospheric artist, dramatic DM, strategic player

See `config.yaml` for full configuration options.

## Resources

- **Full Guide**: `docs/PROMPT_ENGINEERING_GUIDE.md`
- **Architecture**: `docs/FOUR_CHIP_ARCHITECTURE.md`
- **Testing**: `scripts/test-prompt-variants.sh`

