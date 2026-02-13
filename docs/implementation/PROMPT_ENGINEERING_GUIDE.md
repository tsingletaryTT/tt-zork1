# Prompt Engineering Guide for 4-Chip Zork Architecture

**Created**: February 13, 2026
**Status**: Living Document
**Branch**: `feature/multi-llm-integration`

## Table of Contents

1. [Philosophy](#philosophy)
2. [General Principles](#general-principles)
3. [LLM-Specific Guidelines](#llm-specific-guidelines)
4. [Parameter Tuning](#parameter-tuning)
5. [Testing & Iteration](#testing--iteration)
6. [Common Pitfalls](#common-pitfalls)
7. [Optimization Strategies](#optimization-strategies)

---

## Philosophy

### Goal Hierarchy

1. **Correctness**: Output must be accurate and valid
2. **Speed**: Fast enough for interactive gameplay
3. **Quality**: Enhanced experience without degrading base game
4. **Reliability**: Graceful degradation on failure

### Design Principles

- **Single Responsibility**: Each LLM does ONE thing well
- **Fail-Safe**: Game remains playable if any LLM fails
- **User Control**: All enhancements can be toggled
- **Transparent**: User knows when AI is involved

---

## General Principles

### 1. Be Specific and Explicit

❌ **Bad**:
```
You are helpful. Answer questions about Zork.
```

✅ **Good**:
```
You are a command translator for Zork. Convert natural language input
into exact Zork commands using this format: [verb] [object]
```

**Why**: LLMs perform better with clear, narrow tasks.

---

### 2. Use Examples (Few-Shot Learning)

❌ **Bad**:
```
Translate this to a Zork command.
```

✅ **Good**:
```
Examples:
- "open the mailbox" → open mailbox
- "pick up lamp" → take lamp
- "go north" → north

Translate: "I want to examine the painting"
```

**Why**: Qwen3-0.6B learns patterns from examples better than abstract rules.

---

### 3. Constrain Output Format

❌ **Bad**:
```
What should the player do next?
```

✅ **Good**:
```
Respond in JSON:
{
  "command": "single command here",
  "reasoning": "brief explanation",
  "confidence": 0.0-1.0
}
```

**Why**: Structured output is easier to parse and validate.

---

### 4. Set Boundaries

❌ **Bad**:
```
Enhance the game description with details.
```

✅ **Good**:
```
Add 1-2 sentences of atmospheric detail BEFORE the game text.
Never change the game text itself. Maximum 200 characters.
```

**Why**: Prevents output from overwhelming the base game.

---

### 5. Iterate on Real Data

❌ **Bad**:
```
Test with: "open door"
```

✅ **Good**:
```
Test with actual user inputs from play sessions:
- "I want to open the mailbox"
- "Can you pick up the lamp please?"
- "Let's head north"
- "Show me my stuff"
```

**Why**: Real inputs have different patterns than theoretical ones.

---

## LLM-Specific Guidelines

### Command Translator (Device 0, Port 8000)

**Goal**: Convert natural language → Zork commands with 95%+ accuracy

#### Key Challenges

1. **Ambiguity**: "it", "that", "the thing"
2. **Synonyms**: "grab" = "take", "inspect" = "examine"
3. **Politeness**: Remove "please", "can you", "I want to"
4. **Multiple actions**: "go north and take lamp"

#### Prompt Strategy

**Best**: Few-shot with pattern emphasis
```
Examples showing patterns:
"open X" → open X
"pick up/get/grab X" → take X
"go/walk/head DIRECTION" → DIRECTION
```

**Why**: Small models learn patterns better than rules.

#### Parameters

- **Max tokens**: 50-100 (commands are short)
- **Temperature**: 0.1-0.2 (deterministic, no creativity needed)
- **Top-p**: 0.9 (focused, avoid weird outputs)

#### Success Metrics

- **Accuracy**: 95%+ on test set
- **Latency**: < 200ms p95
- **Consistency**: Same input → same output

---

### ASCII Artist (Device 1, Port 8001)

**Goal**: Generate recognizable scene visualizations that enhance immersion

#### Key Challenges

1. **Size constraints**: 60 chars wide, 12 lines tall
2. **ASCII limitations**: Can't draw complex shapes
3. **Consistency**: Same location → similar art
4. **Variety**: Different locations → different art

#### Prompt Strategy

**Best**: Example-based with style guide
```
Style:
- Use ╔═╗║╚╝ for borders
- Use ASCII: /\__/|_\ for structures
- Use emojis sparingly: 🌲🏠📬
- Show depth with spacing

Example for "West of House":
[Show complete example]
```

**Why**: Visual tasks need visual examples.

#### Parameters

- **Max tokens**: 300-500 (art takes space)
- **Temperature**: 0.7-0.9 (creativity needed, but controlled)
- **Top-p**: 0.9-0.95 (allow variety)

#### Success Metrics

- **Recognizability**: Users can identify location
- **Formatting**: Fits within bounds
- **Quality**: Adds atmosphere, not distraction
- **Latency**: < 1000ms p95

---

### Dungeon Master (Device 2, Port 8002)

**Goal**: Add atmospheric flavor without changing game state

#### Key Challenges

1. **Safety**: Never contradict game state
2. **Balance**: Enhance without overwhelming
3. **Context**: Reference recent actions
4. **Variety**: Same location + different actions = different flavor

#### Prompt Strategy

**Best**: Safety-first with examples
```
Add 1-2 sentences BEFORE game text. Never change the game text.

Examples:
Standard: "You are in a forest."
Enhanced: "Shadows dance between ancient trees. You are in a forest."

ALWAYS end with the complete standard text unchanged.
```

**Why**: Emphasizing safety prevents harmful outputs.

#### Parameters

- **Max tokens**: 150-300 (brief enhancements)
- **Temperature**: 0.8-1.0 (creative but safe)
- **Top-p**: 0.95 (allow creative word choice)

#### Success Metrics

- **Safety**: 100% (never breaks game)
- **Quality**: Enhances atmosphere
- **Latency**: < 1500ms p95
- **User satisfaction**: Enable-rate > 60%

---

### AI Player (Device 3, Port 8003)

**Goal**: Make strategic decisions that progress through the game

#### Key Challenges

1. **Strategy**: Long-term planning
2. **Safety**: Avoid death
3. **Exploration**: Systematic mapping
4. **Puzzle solving**: Logical item combinations

#### Prompt Strategy

**Best**: Strategic framework with examples
```
Objectives (in order):
1. SURVIVE: Avoid death (grues, trolls)
2. EXPLORE: Map all locations
3. COLLECT: Gather treasures
4. WIN: Score 350/350

Safety rules:
- NEVER enter dark areas without lamp
- NEVER attack without weapon

Think step-by-step:
1. Assess situation
2. Identify risks
3. Identify opportunities
4. Choose action
```

**Why**: Complex tasks need explicit strategy.

#### Parameters

- **Max tokens**: 300-500 (need reasoning space)
- **Temperature**: 0.6-0.8 (strategic but not random)
- **Top-p**: 0.95 (explore possibilities)

#### Success Metrics

- **Progress**: Increase score over time
- **Safety**: Avoid death >90% of moves
- **Exploration**: Discover new locations
- **Latency**: < 2000ms p95

---

## Parameter Tuning

### Temperature

**What it does**: Controls randomness (0 = deterministic, 2 = chaotic)

**Guidelines**:
- **0.0-0.3**: Deterministic tasks (translation, facts)
- **0.4-0.7**: Balanced (strategic decisions)
- **0.8-1.2**: Creative tasks (art, storytelling)
- **1.3-2.0**: Experimental (usually too random)

**Examples**:
```yaml
translator: 0.15  # Want consistency
artist: 0.8       # Want variety
dm: 0.9           # Want creativity
player: 0.7       # Want strategy with options
```

---

### Max Tokens

**What it does**: Limits output length

**Guidelines**:
- **50-100**: Short outputs (commands, yes/no)
- **100-300**: Medium outputs (descriptions, reasoning)
- **300-500**: Long outputs (complex art, detailed strategy)
- **500+**: Very long (usually unnecessary, increases latency)

**Trade-offs**:
- Higher = More complete outputs, higher latency
- Lower = Faster, risk of truncation

**Sweet spots**:
```yaml
translator: 75    # Commands are ~10 tokens
artist: 400       # ASCII art is ~200-300 tokens
dm: 200           # Brief enhancements
player: 350       # Reasoning + command
```

---

### Top-P (Nucleus Sampling)

**What it does**: Considers only top P% of probability mass

**Guidelines**:
- **0.5-0.8**: Very focused (technical tasks)
- **0.85-0.95**: Balanced (most use cases)
- **0.95-1.0**: Open (creative tasks)

**Default**: 0.95 works well for most tasks

---

### Context Size

**What it does**: How much previous conversation to include

**Guidelines**:
- **Translator**: No context needed (stateless translation)
- **Artist**: Location + objects only
- **DM**: Last 2-3 actions
- **Player**: Full game state

**Trade-offs**:
- More context = Better decisions, slower, more expensive
- Less context = Faster, cheaper, less informed

---

## Testing & Iteration

### Workflow

```
1. Define success metrics
2. Create test dataset
3. Test variant A
4. Test variant B
5. Compare results
6. Pick winner or iterate
7. Repeat
```

### Test Dataset

Create representative test cases:

**Translator**:
```
- Casual: "open mailbox"
- Polite: "could you please take the lamp?"
- Complex: "go north and take sword"
- Ambiguous: "open it"
- Verbose: "I would like to examine the beautiful painting"
```

**Artist**:
```
- Common location: "West of House"
- Dark location: "Cellar"
- Outdoor: "Forest"
- Indoor: "Living Room"
```

**DM**:
```
- Routine move: Standard description
- Discovery: Finding treasure
- Danger: Entering dark area
- Death: Game over scenario
```

**Player**:
```
- Start position: No items, no info
- Mid-game: Some items, partial map
- Puzzle: Stuck on specific challenge
- Combat: Facing enemy
```

### Automated Testing

Use `scripts/test-prompt-variants.sh`:

```bash
# Test all variants
./scripts/test-prompt-variants.sh

# Review results
cd experiments/prompt_variants_YYYYMMDD_HHMMSS/
cat translator_fewshot.txt
cat artist_atmospheric.txt
```

### Manual Testing

Use `scripts/tune-prompt-interactive.sh`:

```bash
./scripts/tune-prompt-interactive.sh

# Select LLM
1

# Test with different inputs
> test open the mailbox
> test pick up lamp
> test go north

# Adjust parameters
> temp 0.2
> tokens 100

# Test again
> test open the mailbox

# Save if better
> save translator_v4_tuned
```

---

## Common Pitfalls

### 1. Over-Specifying

❌ **Problem**:
```
You are a command translator. You must convert natural language
to Zork commands. Zork is a text adventure game from 1977 made
by Infocom. It uses a two-word parser with verb-object syntax...
[300 more words of history and rules]
```

✅ **Solution**:
```
Convert natural language to Zork commands.
Examples: [5-10 examples]
Rules: [3-5 key rules]
```

**Why**: Small models get confused with too much info.

---

### 2. Under-Constraining

❌ **Problem**:
```
Generate ASCII art for a location.
```

✅ **Solution**:
```
Generate ASCII art: 60 chars wide, 10 lines tall.
Use ╔═╗║╚╝ for borders. Example: [show example]
```

**Why**: Without constraints, output is unpredictable.

---

### 3. Ignoring Latency

❌ **Problem**:
```
max_tokens: 2000  # "More is better, right?"
temperature: 1.5   # "Let's get creative!"
```

✅ **Solution**:
```
max_tokens: 75    # Just enough for task
temperature: 0.15  # As low as task allows
```

**Why**: Interactive games need fast responses.

---

### 4. Not Testing Edge Cases

❌ **Problem**:
```
Test: "open mailbox" → ✓
Done!
```

✅ **Solution**:
```
Test: "open mailbox" → ✓
Test: "I want to open the mailbox" → ✓
Test: "open it" → ?
Test: "can you please open the beautiful mailbox?" → ✓
Test: "go north and open mailbox" → ?
```

**Why**: Real users are creative and unpredictable.

---

### 5. Forgetting Graceful Degradation

❌ **Problem**:
```
if llm_fails:
    crash()
```

✅ **Solution**:
```
if llm_fails:
    fallback_to_standard_behavior()
    log_error()
    notify_user_once()
```

**Why**: Game must always be playable.

---

## Optimization Strategies

### 1. Caching

**What**: Store results for repeated inputs

**Where**:
- **Translator**: Cache common phrases
- **Artist**: Cache by location name
- **DM**: Cache by location + action pattern
- **Player**: Don't cache (context-dependent)

**Implementation**:
```c
typedef struct {
    char input[256];
    char output[512];
    time_t timestamp;
    int hits;
} CacheEntry;

CacheEntry* cache_lookup(const char* input);
void cache_store(const char* input, const char* output);
```

**Impact**: 10x faster for cache hits

---

### 2. Batch Requests

**What**: Send multiple requests in parallel

**Example**:
```c
// When location changes, request both in parallel:
pthread_create(&t1, NULL, generate_ascii_art, location);
pthread_create(&t2, NULL, generate_dm_flavor, location);

pthread_join(t1, NULL);
pthread_join(t2, NULL);

// Total time = max(art_time, dm_time) instead of sum!
```

**Impact**: 2x faster for parallel-eligible requests

---

### 3. Progressive Enhancement

**What**: Show basic output first, enhance later

**Example**:
```
Display game text immediately
↓
Display ASCII art when ready (500ms later)
↓
Display DM enhancement when ready (1000ms later)
```

**Why**: Maintains interactivity

---

### 4. Adaptive Quality

**What**: Reduce quality when latency is high

**Example**:
```c
if (last_request_ms > 1000) {
    // Latency too high, reduce quality
    max_tokens = max_tokens * 0.75;
    temperature = temperature * 0.9;
}
```

**Why**: Better to have fast, good results than slow, perfect results

---

### 5. Smart Triggers

**What**: Only invoke LLMs when needed

**Examples**:
- **Artist**: Only on location change, not every output
- **DM**: Only on important moments (discovery, danger)
- **Player**: Only when explicitly activated

**Impact**: Reduce LLM calls by 70%

---

## Experimentation Log

Keep a log of experiments:

```markdown
## Experiment: Translator Temperature Sweep

**Date**: 2026-02-13
**Goal**: Find optimal temperature for translation accuracy

**Test**: 20 diverse inputs with temperatures 0.0, 0.1, 0.2, 0.3

**Results**:
- temp 0.0: 95% accuracy, 100% consistency
- temp 0.1: 96% accuracy, 100% consistency
- temp 0.2: 94% accuracy, 95% consistency
- temp 0.3: 91% accuracy, 90% consistency

**Conclusion**: Use temp 0.1 (best accuracy + consistency)
**Updated**: config.yaml → translator.recommended.temperature = 0.1
```

---

## Resources

### Tools
- `scripts/test-prompt-variants.sh` - Automated testing
- `scripts/tune-prompt-interactive.sh` - Interactive tuning
- `prompts/config.yaml` - Configuration file

### Directories
- `prompts/translator/` - Command translation prompts
- `prompts/artist/` - ASCII art prompts
- `prompts/dm/` - Dungeon master prompts
- `prompts/player/` - AI player prompts
- `experiments/` - Test results and logs

### References
- [OpenAI Prompt Engineering Guide](https://platform.openai.com/docs/guides/prompt-engineering)
- [Anthropic Prompt Engineering](https://docs.anthropic.com/claude/docs/prompt-engineering)
- [Qwen3 Model Card](https://huggingface.co/Qwen/Qwen3-0.6B)

---

## Next Steps

1. ✅ Create prompt variants
2. ✅ Create testing scripts
3. ⏳ Run initial tests on hardware
4. ⏳ Analyze results
5. ⏳ Iterate and optimize
6. ⏳ Document findings
7. ⏳ Update config.yaml with winners

**Start here**: `./scripts/start-four-llms.sh` then `./scripts/test-prompt-variants.sh`

---

**Remember**: Prompt engineering is iterative. Start simple, test with real data, and refine based on results. The "best" prompt is the one that works for your specific use case!
