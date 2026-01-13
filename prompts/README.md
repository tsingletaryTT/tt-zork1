# Zork LLM Prompts

This directory contains the prompts used by the LLM-backed natural language parser for Zork.

## Educational Purpose

**IMPORTANT**: This is an educational project! These prompts are stored as plain text files so you can:
- Easily read and understand how the LLM is instructed
- Experiment with different prompting strategies
- Modify behavior without recompiling the code
- Learn about prompt engineering for text adventure games

## Files

### `system.txt`
The **system prompt** that defines the LLM's role and behavior. This is sent once at the beginning of each API call.

**What it does:**
- Tells the LLM it's a command translator
- Provides rules for translation
- Gives examples of good translations

**Prompt engineering tips:**
- Keep rules concise and numbered for clarity
- Provide diverse examples covering common patterns
- Emphasize "output ONLY commands" to avoid chatty responses
- Use classic Zork command syntax in examples

### `user_template.txt`
The **user message template** that includes game context and the player's input.

**Placeholders:**
- `{CONTEXT}` - Replaced with recent game history (last N turns of output and input)
- `{INPUT}` - Replaced with the user's natural language input

**Prompt engineering tips:**
- Label sections clearly: `[GAME CONTEXT]`, `[USER INPUT]`
- Experiment with context length (currently "Last 20 turns")
- Try different instruction phrasings: "Translate", "Convert", "Output commands for"
- Add constraints if needed: "Use at most 3 commands"

## How It Works

```
1. User types: "I want to go north and open the door"

2. prompt_loader.c reads these files

3. Substitutes placeholders:
   {CONTEXT} → Recent game output
   {INPUT} → "I want to go north and open the door"

4. Sends to LLM:
   System: [Contents of system.txt]
   User: [Contents of user_template.txt with substitutions]

5. LLM responds: "north, open door"

6. Commands executed in Z-machine
```

## Experimenting

Try modifying these prompts to change behavior:

### Make it more verbose
```
User: "walk through the door"
You: "open door, north, look"
(Added automatic "look" after movement)
```

### Add personality
```
You are a helpful Zork assistant who translates commands.
Respond with just the commands, then add a single emoji.
```

### Stricter parsing
```
If the request is impossible or unclear, respond with: "unclear"
```

### Context-aware suggestions
```
Based on visible objects in context, suggest taking important items first.
```

## Technical Details

**Loading**: Prompts are loaded at startup by `src/llm/prompt_loader.c`

**Fallback**: If files are missing, built-in default prompts are used

**No recompilation**: Edit these files and restart the game - changes take effect immediately!

## Advanced: Creating Custom Prompts

Want to create a different personality or style?

1. Copy these files:
   ```bash
   cp system.txt system_helpful.txt
   cp system.txt system_terse.txt
   ```

2. Modify the copies with different instructions

3. Set environment variable to use your custom prompts:
   ```bash
   export ZORK_SYSTEM_PROMPT=prompts/system_terse.txt
   ./zork-native game/zork1.z3
   ```

## Learning Resources

**Prompt Engineering Basics:**
- [OpenAI Prompt Engineering Guide](https://platform.openai.com/docs/guides/prompt-engineering)
- [Anthropic's Guide to Prompting](https://docs.anthropic.com/claude/docs/prompt-engineering)

**Key Concepts:**
- **System prompt**: Sets the AI's role and behavior
- **Few-shot learning**: Examples teach the AI the pattern
- **Temperature**: Lower = more consistent, Higher = more creative (set in API call)
- **Context window**: How much history the AI can see (we send ~20 turns)

**Experiment and Learn:**
The best way to understand prompting is to try it! Modify these files, play the game, and see what happens. That's the whole point of making them editable text files.

## Troubleshooting

**LLM gives wrong commands:**
- Add more examples to `system.txt` covering that case
- Check if game context in `user_template.txt` is sufficient
- Try being more explicit in the rules

**LLM is too chatty:**
- Emphasize "output ONLY commands" in system prompt
- Add negative examples: "Don't say 'Here are the commands:' "
- Try stronger language: "You MUST output only commands, no other text"

**LLM misses context clues:**
- Increase context length in user template
- Explicitly tell LLM to "read the game context carefully"
- Add instruction: "Use object names visible in the context"

Have fun experimenting! 🎮
