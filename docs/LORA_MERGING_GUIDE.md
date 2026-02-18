# LoRA Merging for Everyone: Make Specialized AI Models

**A Non-Technical Guide to Creating Your Own Specialized Models**

---

## 🎨 What You'll Learn

By the end of this guide, you'll be able to:
- Take any base AI model (like Llama-3.2-1B)
- Find specialized "adapters" on HuggingFace (ASCII art, poetry, code, etc.)
- Merge them together to create your own specialized model
- Use your custom model just like any other AI model

**No AI/ML expertise required!** Just follow the recipe.

---

## 📖 Table of Contents

1. [What is LoRA Merging? (The Simple Explanation)](#what-is-lora-merging)
2. [Why Would I Want to Do This?](#why-would-i-want-to-do-this)
3. [What You Need (Prerequisites)](#what-you-need)
4. [Finding Compatible Models on HuggingFace](#finding-compatible-models)
5. [Step-by-Step: Merge Your First LoRA](#step-by-step-merge-your-first-lora)
6. [Real-World Examples](#real-world-examples)
7. [Troubleshooting](#troubleshooting)
8. [Advanced Tips](#advanced-tips)

---

## What is LoRA Merging?

### The Restaurant Analogy

Think of AI models like chefs:

- **Base Model** = A trained chef who knows general cooking (Llama-3.2-1B-Instruct)
- **LoRA Adapter** = A specialized recipe book (e.g., "French Pastry Techniques")
- **Merged Model** = The chef with the recipes permanently memorized

**Without merging:**
- Chef has to constantly look up recipes (slower, needs extra book)

**With merging:**
- Chef has recipes memorized (faster, no extra dependency)

### The Technical Truth (But Simple)

- **LoRA** (Low-Rank Adaptation) = Small file that teaches a model new skills
- **Merging** = Permanently adding those skills to the base model
- **Result** = One model that can do everything, no separate files needed

**Key Benefit:** You get a specialized model without the complexity of managing separate adapter files.

---

## Why Would I Want to Do This?

### 1. **Create Specialized Models**

Turn a general-purpose model into a specialist:
- 🎨 ASCII art generator (what we just did!)
- 📝 Poetry writer
- 💻 Code assistant
- 🎭 Character roleplay
- 🌍 Language translator
- 🎵 Lyrics generator

### 2. **Combine Multiple Skills**

You can merge multiple LoRAs sequentially:
- Base model → Add ASCII art → Add poetry → Result: AI that writes poems with ASCII art!

### 3. **No Runtime Overhead**

- LoRA adapters need special loading code
- Merged models work like any normal model
- Simpler deployment, faster inference

### 4. **Distribution**

- Share one file instead of "base + adapter"
- Easier for others to use your creation

### 5. **Cost & Speed**

- Smaller specialized models (1B-3B params) can outperform generic large models (7B-13B) at specific tasks
- Cheaper to run, faster responses

---

## What You Need

### Required Software

1. **Python 3.8+** (check: `python3 --version`)
2. **pip** (Python package installer)
3. **Git** (for downloading models)

### Required Python Packages

```bash
pip install transformers peft torch
```

**What these do:**
- `transformers` - Loads AI models from HuggingFace
- `peft` - Handles LoRA adapters
- `torch` - PyTorch (the AI framework)

### Hardware Requirements

**Minimum:**
- 8GB RAM (for 1B parameter models)
- 10GB free disk space

**Recommended:**
- 16GB RAM (for 3B parameter models)
- 20GB free disk space
- GPU (optional, makes it faster)

**Time:**
- 1B models: ~5-10 minutes to merge
- 3B models: ~10-20 minutes
- 7B models: ~20-40 minutes (needs 32GB RAM!)

---

## Finding Compatible Models on HuggingFace

### Step 1: Know Your Base Model

You need to know what base model you're starting with. Common choices:
- `meta-llama/Llama-3.2-1B-Instruct` (1 billion parameters, fast)
- `meta-llama/Llama-3.2-3B-Instruct` (3 billion, more capable)
- `mistralai/Mistral-7B-Instruct-v0.3` (7 billion, very capable)

**Where to find base models:**
https://huggingface.co/models?pipeline_tag=text-generation&sort=downloads

### Step 2: Search for LoRA Adapters

**On HuggingFace:**
https://huggingface.co/models?library=peft&sort=downloads

**Search tips:**
- Filter by "PEFT" or "LoRA" in the model card
- Look for "adapter" in the model name
- Check the model card for "Base Model" compatibility

### Step 3: Check Compatibility

**CRITICAL:** LoRA must match base model architecture!

**How to check:**
1. Open the LoRA adapter page on HuggingFace
2. Look for "Base Model" in the model card
3. Verify it matches your chosen base model

**Example (our ASCII art case):**
- Base Model: `meta-llama/Llama-3.2-1B-Instruct`
- LoRA Adapter: `AvaLovelace/LLaMA-ASCII-Art`
- Model Card Says: "Base Model: meta-llama/Llama-3.2-1B-Instruct" ✅ COMPATIBLE

**Warning Signs (NOT compatible):**
- Different model families (Llama vs Mistral)
- Different sizes (1B vs 3B)
- Different architectures (GPT vs Llama)

### Popular LoRA Adapters by Category

#### 🎨 Creative Arts
- `AvaLovelace/LLaMA-ASCII-Art` - ASCII art generation
- `[username]/emoji-artist` - Emoji art
- `[username]/pixel-art-generator` - Pixel art

#### 📝 Writing & Language
- `[username]/poetry-writer` - Poetry generation
- `[username]/story-teller` - Creative fiction
- `[username]/translator-es` - Spanish translation

#### 💻 Code & Tech
- `[username]/python-expert` - Python code generation
- `[username]/sql-helper` - SQL query writing
- `[username]/bash-assistant` - Shell scripting

#### 🎭 Roleplay & Characters
- `[username]/shakespeare` - Shakespearean dialogue
- `[username]/pirate-speak` - Pirate character
- `[username]/dungeon-master` - D&D game master

**Note:** Replace `[username]` with actual HuggingFace usernames. Search for these on HuggingFace.

---

## Step-by-Step: Merge Your First LoRA

### Method 1: Using Our Script (Easiest)

**1. Get the merge script:**

If you're in the tt-zork1 project:
```bash
# Already have it!
python scripts/merge_lora.py
```

If you're starting fresh:
```bash
# Create merge_lora.py (see appendix for full script)
curl -O https://raw.githubusercontent.com/anthropics/claude-code/main/examples/merge_lora.py
```

**2. Run the merge:**

```bash
python scripts/merge_lora.py \
    <base_model> \
    <lora_adapter> \
    <output_directory>
```

**Example (ASCII art):**
```bash
python scripts/merge_lora.py \
    meta-llama/Llama-3.2-1B-Instruct \
    AvaLovelace/LLaMA-ASCII-Art \
    ./models/my-ascii-artist
```

**3. Wait for completion:**
```
======================================================================
  LoRA Adapter Merge Tool
======================================================================

[1/5] Loading base model: meta-llama/Llama-3.2-1B-Instruct
  ✓ Loaded meta-llama/Llama-3.2-1B-Instruct

[2/5] Loading LoRA adapter: AvaLovelace/LLaMA-ASCII-Art
  ✓ Loaded adapter from AvaLovelace/LLaMA-ASCII-Art

[3/5] Merging LoRA weights into base model...
  ✓ Successfully merged adapter weights

[4/5] Saving merged model to: ./models/my-ascii-artist
  ✓ Model saved

[5/5] Saving tokenizer...
  ✓ Tokenizer saved

======================================================================
  ✅ SUCCESS! LoRA adapter merged into base model
======================================================================
```

**4. Your merged model is ready!**
- Location: `./models/my-ascii-artist/`
- Size: ~2-5GB (depending on base model size)
- Can be used like any HuggingFace model

### Method 2: Python Script (More Control)

**Create merge.py:**
```python
#!/usr/bin/env python3
from transformers import AutoModelForCausalLM, AutoTokenizer
from peft import PeftModel

# Configuration
BASE_MODEL = "meta-llama/Llama-3.2-1B-Instruct"
LORA_ADAPTER = "AvaLovelace/LLaMA-ASCII-Art"
OUTPUT_DIR = "./models/my-ascii-artist"

# Step 1: Load base model
print(f"Loading base model: {BASE_MODEL}")
base_model = AutoModelForCausalLM.from_pretrained(
    BASE_MODEL,
    device_map="auto",
    trust_remote_code=True
)

# Step 2: Load LoRA adapter on top
print(f"Loading LoRA adapter: {LORA_ADAPTER}")
model = PeftModel.from_pretrained(
    base_model,
    LORA_ADAPTER,
    device_map="auto"
)

# Step 3: Merge adapter into base model
print("Merging LoRA weights...")
merged_model = model.merge_and_unload()

# Step 4: Save merged model
print(f"Saving to: {OUTPUT_DIR}")
merged_model.save_pretrained(OUTPUT_DIR)

# Step 5: Save tokenizer
tokenizer = AutoTokenizer.from_pretrained(BASE_MODEL)
tokenizer.save_pretrained(OUTPUT_DIR)

print("✅ Done! Merged model saved.")
```

**Run it:**
```bash
python merge.py
```

---

## Real-World Examples

### Example 1: ASCII Art Generator (What We Just Did)

**Goal:** Create a model that generates thematic ASCII art for text adventure games

**Base Model:** `meta-llama/Llama-3.2-1B-Instruct` (1B params, fast)
**LoRA:** `AvaLovelace/LLaMA-ASCII-Art`
**Result:** Specialized ASCII artist

**Command:**
```bash
python scripts/merge_lora.py \
    meta-llama/Llama-3.2-1B-Instruct \
    AvaLovelace/LLaMA-ASCII-Art \
    ~/models/Llama-3.2-1B-ASCII-merged
```

**Use Case:**
- Text adventure games (Zork)
- Terminal UI art
- Retro game graphics
- Fun Discord bot responses

**Before (Base Model):**
```
User: Generate ASCII art for a forest
Base Model: Sure! Here's a forest:
    tree
    tree
```

**After (Merged LoRA):**
```
User: Generate ASCII art for a forest
Merged Model:
    🌲🌲🌲🌲🌲
   🌲🌳🌲🌳🌲
  🌲🌿🌲🌿🌲🌲
 🌿🌿🌿🌿🌿🌿
```

### Example 2: Poetry + Style

**Goal:** Model that writes poetry in specific styles

**Base:** `meta-llama/Llama-3.2-3B-Instruct`
**LoRA 1:** `username/poetry-writer`
**LoRA 2:** `username/haiku-master`

**Sequential Merge:**
```bash
# Step 1: Merge poetry writer
python scripts/merge_lora.py \
    meta-llama/Llama-3.2-3B-Instruct \
    username/poetry-writer \
    ./temp-poetry

# Step 2: Merge haiku on top
python scripts/merge_lora.py \
    ./temp-poetry \
    username/haiku-master \
    ./models/haiku-poet
```

**Result:** Model specializing in haiku poetry

### Example 3: Multilingual Assistant

**Goal:** Model that speaks technical English + Spanish

**Base:** `meta-llama/Llama-3.2-1B-Instruct`
**LoRA:** `username/spanish-translator`

**Command:**
```bash
python scripts/merge_lora.py \
    meta-llama/Llama-3.2-1B-Instruct \
    username/spanish-translator \
    ./models/bilingual-assistant
```

**Use Case:**
- Customer support (bilingual)
- Translation tools
- Language learning apps

### Example 4: Code Specialist

**Goal:** Model that excels at Python + documentation

**Base:** `mistralai/Mistral-7B-Instruct-v0.3`
**LoRA:** `username/python-expert`

**Command:**
```bash
python scripts/merge_lora.py \
    mistralai/Mistral-7B-Instruct-v0.3 \
    username/python-expert \
    ./models/python-assistant
```

**Result:** Better Python code generation than base model

---

## Troubleshooting

### Problem: "ModuleNotFoundError: No module named 'peft'"

**Solution:**
```bash
pip install peft transformers torch
```

### Problem: "CUDA out of memory"

**Cause:** Model too large for your GPU

**Solutions:**
1. Use CPU-only (slower but works):
   ```python
   base_model = AutoModelForCausalLM.from_pretrained(
       BASE_MODEL,
       device_map="cpu",  # Force CPU
       trust_remote_code=True
   )
   ```

2. Use a smaller base model (3B → 1B)

3. Increase swap space (Linux):
   ```bash
   sudo fallocate -l 16G /swapfile
   sudo chmod 600 /swapfile
   sudo mkswap /swapfile
   sudo swapon /swapfile
   ```

### Problem: "RuntimeError: PeftModel requires a model with a supported architecture"

**Cause:** LoRA adapter not compatible with base model

**Solution:** Double-check compatibility:
- Same model family? (Llama with Llama, not Llama with Mistral)
- Same size? (1B with 1B, not 1B with 3B)
- Check LoRA's model card for "Base Model" field

### Problem: Merge completes but model generates gibberish

**Causes:**
1. Incompatible LoRA (wrong base model)
2. Corrupted download
3. LoRA trained poorly

**Solutions:**
1. Verify compatibility (see above)
2. Re-download:
   ```bash
   rm -rf ~/.cache/huggingface/hub
   # Try merge again
   ```
3. Try a different LoRA adapter

### Problem: "OSError: Not enough disk space"

**Solution:** Models need ~2-5GB per 1B parameters

Check space:
```bash
df -h
```

Free space:
```bash
# Delete old models
rm -rf ./models/old-model

# Or use external drive
python scripts/merge_lora.py ... /mnt/external/models/my-model
```

### Problem: "ImportError: cannot import name 'merge_and_unload'"

**Cause:** Old version of `peft`

**Solution:**
```bash
pip install --upgrade peft transformers
```

---

## Advanced Tips

### Tip 1: Check Model Quality Before Full Merge

Test the LoRA before merging:

```python
from transformers import AutoModelForCausalLM, AutoTokenizer
from peft import PeftModel

# Load base + adapter (NOT merged)
base = AutoModelForCausalLM.from_pretrained("base-model")
model = PeftModel.from_pretrained(base, "lora-adapter")
tokenizer = AutoTokenizer.from_pretrained("base-model")

# Test it
inputs = tokenizer("Test prompt", return_tensors="pt")
outputs = model.generate(**inputs, max_new_tokens=50)
print(tokenizer.decode(outputs[0]))

# If output is good, proceed with merge
```

### Tip 2: Merge Multiple LoRAs Sequentially

**Goal:** Combine multiple skills

```bash
# Start with base model
BASE="meta-llama/Llama-3.2-1B-Instruct"

# Merge skill 1
python scripts/merge_lora.py $BASE lora1 ./temp1

# Merge skill 2 on top
python scripts/merge_lora.py ./temp1 lora2 ./temp2

# Merge skill 3 on top
python scripts/merge_lora.py ./temp2 lora3 ./final-model

# Clean up
rm -rf ./temp1 ./temp2
```

**Example:** Base → Poetry → Haiku → Rhyme = Specialized poetry model

### Tip 3: Quantize After Merging (Smaller Files)

Make your merged model smaller:

```bash
# Install quantization tool
pip install bitsandbytes

# Quantize to 8-bit (2x smaller)
python -c "
from transformers import AutoModelForCausalLM
model = AutoModelForCausalLM.from_pretrained(
    './models/my-model',
    load_in_8bit=True
)
model.save_pretrained('./models/my-model-8bit')
"
```

**Trade-off:** Smaller size, slightly lower quality

### Tip 4: Upload Your Merged Model to HuggingFace

Share your creation!

```bash
# Install HuggingFace CLI
pip install huggingface_hub

# Login
huggingface-cli login

# Upload
huggingface-cli upload your-username/model-name ./models/my-model
```

**Benefits:**
- Others can use your merged model
- Easy to download from anywhere
- Version control

### Tip 5: Create Model Cards

Document what your model does:

Create `README.md` in your model directory:
```markdown
# My Specialized Model

## Description
This is a [base model] merged with [LoRA adapter] for [specific task].

## Base Model
- meta-llama/Llama-3.2-1B-Instruct

## LoRA Adapter
- AvaLovelace/LLaMA-ASCII-Art

## Use Cases
- ASCII art generation
- Text adventure games
- Terminal UI decoration

## Example Usage
\`\`\`python
from transformers import AutoModelForCausalLM, AutoTokenizer

model = AutoModelForCausalLM.from_pretrained("your-username/model-name")
tokenizer = AutoTokenizer.from_pretrained("your-username/model-name")

prompt = "Generate ASCII art for a castle"
inputs = tokenizer(prompt, return_tensors="pt")
outputs = model.generate(**inputs, max_new_tokens=100)
print(tokenizer.decode(outputs[0]))
\`\`\`

## Training Details
- Merged on: 2026-02-17
- Hardware: [Your setup]
- Merge time: ~10 minutes
```

---

## Appendix: Complete Merge Script

**Save as `merge_lora.py`:**

```python
#!/usr/bin/env python3
"""
merge_lora.py - Merge LoRA adapter weights into base model

Usage:
    python merge_lora.py <base_model> <lora_adapter> <output_dir>

Example:
    python merge_lora.py \
        meta-llama/Llama-3.2-1B-Instruct \
        AvaLovelace/LLaMA-ASCII-Art \
        ./models/my-ascii-artist

Requirements:
    pip install transformers peft torch
"""

import sys
import os
from pathlib import Path

def main():
    if len(sys.argv) != 4:
        print(__doc__)
        sys.exit(1)

    base_model_id = sys.argv[1]
    lora_adapter_id = sys.argv[2]
    output_dir = sys.argv[3]

    # Check dependencies
    try:
        from transformers import AutoModelForCausalLM, AutoTokenizer
        from peft import PeftModel
    except ImportError as e:
        print(f"❌ Missing dependencies: {e}")
        print("\nInstall with:")
        print("  pip install transformers peft torch")
        sys.exit(1)

    # Create output directory
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    print("=" * 70)
    print("  LoRA Adapter Merge Tool")
    print("=" * 70)
    print()

    # Step 1: Load base model
    print(f"[1/5] Loading base model: {base_model_id}")
    try:
        base_model = AutoModelForCausalLM.from_pretrained(
            base_model_id,
            device_map="auto",
            trust_remote_code=True
        )
        print(f"  ✓ Loaded {base_model_id}")
    except Exception as e:
        print(f"  ✗ Failed to load base model: {e}")
        sys.exit(1)

    # Step 2: Load LoRA adapter
    print(f"\n[2/5] Loading LoRA adapter: {lora_adapter_id}")
    try:
        model = PeftModel.from_pretrained(
            base_model,
            lora_adapter_id,
            device_map="auto"
        )
        print(f"  ✓ Loaded adapter from {lora_adapter_id}")
    except Exception as e:
        print(f"  ✗ Failed to load LoRA adapter: {e}")
        sys.exit(1)

    # Step 3: Merge
    print(f"\n[3/5] Merging LoRA weights into base model...")
    try:
        merged_model = model.merge_and_unload()
        print(f"  ✓ Successfully merged adapter weights")
    except Exception as e:
        print(f"  ✗ Merge failed: {e}")
        sys.exit(1)

    # Step 4: Save model
    print(f"\n[4/5] Saving merged model to: {output_dir}")
    try:
        merged_model.save_pretrained(output_dir)
        print(f"  ✓ Model saved")
    except Exception as e:
        print(f"  ✗ Failed to save model: {e}")
        sys.exit(1)

    # Step 5: Save tokenizer
    print(f"\n[5/5] Saving tokenizer...")
    try:
        tokenizer = AutoTokenizer.from_pretrained(base_model_id)
        tokenizer.save_pretrained(output_dir)
        print(f"  ✓ Tokenizer saved")
    except Exception as e:
        print(f"  ✗ Failed to save tokenizer: {e}")
        sys.exit(1)

    # Success
    print()
    print("=" * 70)
    print("  ✅ SUCCESS! LoRA adapter merged into base model")
    print("=" * 70)
    print()
    print(f"Output location: {output_dir}")
    print()
    print("Use with:")
    print(f"  python -c \"from transformers import AutoModelForCausalLM; \\")
    print(f"    model = AutoModelForCausalLM.from_pretrained('{output_dir}')\"")
    print()

if __name__ == "__main__":
    main()
```

**Make it executable:**
```bash
chmod +x merge_lora.py
```

---

## Resources

### Learning More

**HuggingFace Docs:**
- Models: https://huggingface.co/docs/transformers/model_doc
- PEFT: https://huggingface.co/docs/peft
- LoRA: https://huggingface.co/docs/peft/conceptual_guides/lora

**Community:**
- HuggingFace Forums: https://discuss.huggingface.co
- Reddit: r/LocalLLaMA
- Discord: HuggingFace server

### Finding Models

**Popular Model Hubs:**
- HuggingFace: https://huggingface.co/models
- Civitai (art-focused): https://civitai.com
- Kaggle: https://www.kaggle.com/models

**Search Tips:**
- Sort by downloads (most popular)
- Filter by task (text-generation, image-generation, etc.)
- Read model cards carefully
- Check license (some are non-commercial only)

---

## Conclusion

**You now know how to:**
- ✅ Find compatible LoRA adapters on HuggingFace
- ✅ Merge LoRAs into base models
- ✅ Create specialized models for specific tasks
- ✅ Troubleshoot common issues
- ✅ Share your creations with the community

**Next Steps:**
1. Try merging a simple LoRA (ASCII art is a good start)
2. Test your merged model
3. Experiment with different LoRAs
4. Create and share your own LoRA-merged models!

**The Possibilities:**
- 🎨 Art generators (ASCII, emoji, pixel art)
- 📝 Writing assistants (poetry, stories, technical docs)
- 💻 Code helpers (Python, JavaScript, SQL)
- 🎭 Character roleplay (D&D, historical figures)
- 🌍 Translation tools (multilingual support)
- 🎵 Creative generation (lyrics, scripts, jokes)

**Have fun creating!** 🚀

---

*Guide created: 2026-02-17*
*For: tt-zork1 project*
*By: Claude (Sonnet 4.5)*
