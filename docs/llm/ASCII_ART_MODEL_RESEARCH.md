# ASCII Art Model Research: Findings & Recommendations

## Current Status (As of 2026-02-17)

**Models Tested:**
1. ✅ **Qwen3-0.6B** (instruct) - FAILED: Generates thinking text instead of art
2. ✅ **Qwen2.5-Coder-0.5B** (code model) - FAILED: Generates descriptive text

**Result:** Both models optimized for natural language output fail at visual pattern generation.

---

## Finding: AvaLovelace/LLaMA-ASCII-Art 🎯

### Overview

**The ONLY text-generation model specifically fine-tuned for ASCII art on HuggingFace.**

**Model Details:**
- **Base:** meta-llama/Llama-3.2-1B-Instruct
- **Type:** LoRA adapter (PEFT)
- **Parameters:** 1 billion (base model) + ~33M LoRA parameters
- **Format:** adapter_model.safetensors (~130 MB)
- **License:** MIT
- **Training:** 10 epochs on [ASCII-Art dataset](https://huggingface.co/datasets/AvaLovelace/ASCII-Art)
- **Hardware Used:** 1× NVIDIA RTX A6000 (1 hour training)

**Created by:** 15-780 Final Project at CMU

### How It Works

This is a **LoRA (Low-Rank Adaptation) model** that needs to be loaded on top of the base Llama-3.2-1B-Instruct model:

```
Base Model (1B params, ~2.5 GB)
    +
LoRA Adapter (~33M params, ~130 MB)
    =
ASCII Art Generator (~2.6 GB total)
```

### Advantages ✅

1. **Purpose-built:** Only model specifically trained for ASCII art text generation
2. **Small enough:** 2.6 GB total fits comfortably on single Tenstorrent chip
3. **vLLM compatible:** vLLM supports LoRA adapters with `--enable-lora` flag
4. **Well-documented:** Clear training methodology and dataset
5. **Proven results:** Successfully fine-tuned with measurable improvement
6. **MIT license:** No usage restrictions

### Implementation Requirements

**Download sizes:**
- Base model: ~2.5 GB (meta-llama/Llama-3.2-1B-Instruct)
- LoRA adapter: ~130 MB (AvaLovelace/LLaMA-ASCII-Art)
- **Total:** ~2.6 GB

**Steps:**
1. Download base model: `tt model get meta-llama/Llama-3.2-1B-Instruct`
2. Download LoRA adapter: `tt model get AvaLovelace/LLaMA-ASCII-Art`
3. Start server with LoRA support:
   ```bash
   tt serve start meta-llama/Llama-3.2-1B-Instruct \
     --port 8001 \
     --device-id 1 \
     --enable-lora \
     --lora-modules ascii-art=AvaLovelace/LLaMA-ASCII-Art \
     --max-model-len 2048 \
     --tensor-parallel 1 \
     --detach
   ```
4. Update config to use LoRA adapter in requests
5. Test with existing `./test_artist` program

**Estimated time:** 1-2 hours
- Download: 15-20 minutes
- Configuration: 15 minutes
- Testing: 30-60 minutes

### Potential Challenges ⚠️

1. **LoRA setup complexity:** May need to adjust vLLM server launch flags
2. **Request format:** Need to specify LoRA adapter in API calls
3. **Larger than current:** 2.6 GB vs 1.4 GB (but still fits single chip)
4. **Unknown quality:** Haven't seen actual output examples from this model

---

## Alternative Small Models Investigated

### SmolLM2-1.7B
- **Size:** 1.7B parameters (~3.4 GB)
- **Status:** General-purpose, not ASCII-trained
- **Verdict:** ❌ Likely same issues as Qwen models

### Shakti-500M
- **Size:** 500M parameters (~1 GB)
- **Status:** General-purpose SLM
- **Verdict:** ❌ Not trained for ASCII art

### Qwen2.5-1.5B
- **Size:** 1.5B parameters (~3 GB)
- **Status:** Multilingual general-purpose
- **Verdict:** ❌ Not specialized for ASCII art

**Conclusion:** No other small (<2GB) ASCII-trained text-generation models found.

---

## Other ASCII Art Approaches

### 1. CiroN2022/ascii-art (Image Generation)
- **Type:** Stable Diffusion XL LoRA
- **Size:** 6.9 GB (SDXL base) + 100-200 MB (LoRA)
- **Output:** PNG/JPEG images styled as ASCII art
- **Complexity:** HIGH - Different pipeline, image-to-text conversion needed
- **Time:** 10-30 seconds per generation
- **Verdict:** ⏭️ Too complex for initial implementation

### 2. Fine-tune Existing Model
- **Dataset:** [mrzjy/ascii_art_generation_140k](https://huggingface.co/datasets/mrzjy/ascii_art_generation_140k)
- **Base model:** Any small LLM (Qwen3-0.6B, etc.)
- **Effort:** 8-16 hours (setup + training + testing)
- **Requirements:** GPU with 16+ GB VRAM
- **Verdict:** ⏭️ Too much effort for MVP

### 3. Placeholder System (Simple Fallback)
- **Implementation:** Keyword → emoji/ASCII pattern mapping
- **Examples:**
  ```
  "West of House" → 🏠 | "Forest" → 🌲🌳 | "Cave" → 🏔️💀
  ```
- **Effort:** 1-2 hours
- **Quality:** Low variety, but reliable
- **Verdict:** ✅ Good fallback if LLaMA-ASCII-Art fails

---

## Recommendation: Try AvaLovelace/LLaMA-ASCII-Art ⭐

### Why It's Worth Trying

1. **Only specialized option available** - All other small models fail at ASCII art
2. **Reasonable size** - 2.6 GB total fits single chip comfortably
3. **Proven training** - Fine-tuned specifically for this task
4. **Quick test** - 1-2 hours to know if it works
5. **Worst case** - Fall back to placeholder system if it fails

### Success Criteria

**Minimum viable:**
- Generates ANY ASCII art patterns (emojis, box characters, etc.)
- Less than 50% thinking text in output
- At least 3-4 lines of actual art

**Ideal outcome:**
- Clean ASCII art with minimal/no thinking text
- Thematically appropriate to location
- Readable and visually distinct

### Fallback Plan

If LLaMA-ASCII-Art doesn't work well:
1. Implement simple placeholder system (1-2 hours)
2. Continue with DM and Player agents (high priority)
3. Revisit artist in Phase 2 with:
   - Image generation approach (SDXL + converter)
   - Custom fine-tuning on larger GPU
   - Alternative small ASCII-trained models if any emerge

---

## Research Sources

- [AvaLovelace/LLaMA-ASCII-Art on HuggingFace](https://huggingface.co/AvaLovelace/LLaMA-ASCII-Art)
- [AvaLovelace/ASCII-Art Dataset](https://huggingface.co/datasets/AvaLovelace/ASCII-Art)
- [vLLM LoRA Adapter Documentation](https://docs.vllm.ai/en/stable/features/lora/)
- [mrzjy ASCII Art Generation Dataset](https://huggingface.co/datasets/mrzjy/ascii_art_generation_140k)
- [Small Language Models Overview](https://huggingface.co/blog/jjokah/small-language-model)
- [Why LLMs Suck at ASCII Art](https://medium.com/data-science/why-llms-suck-at-ascii-art-a9516cb880d5)
- [CiroN2022/ascii-art SDXL LoRA](https://huggingface.co/CiroN2022/ascii-art)

---

## Summary Table

| Model | Type | Size | ASCII-Trained? | Single-Chip? | Effort | Status |
|-------|------|------|----------------|--------------|--------|--------|
| Qwen3-0.6B | LLM | 1.4 GB | ❌ | ✅ | 0h (tested) | ❌ FAILED |
| Qwen2.5-Coder-0.5B | LLM | 1.0 GB | ❌ | ✅ | 0h (tested) | ❌ FAILED |
| **AvaLovelace/LLaMA-ASCII-Art** | **LLM+LoRA** | **2.6 GB** | **✅** | **✅** | **1-2h** | **⭐ RECOMMENDED** |
| CiroN2022/ascii-art | SD-XL | 7 GB | ✅ (images) | ✅ | 4-6h | ⏭️ Too complex |
| Custom fine-tune | LLM | 1-3 GB | ✅ (DIY) | ✅ | 8-16h | ⏭️ Too much work |
| Placeholder system | Code | 0 MB | N/A | ✅ | 1-2h | ✅ Good fallback |

**Verdict:** Try AvaLovelace/LLaMA-ASCII-Art first. If it doesn't work, implement placeholder system and move on to DM/Player agents.

---

*Updated: 2026-02-17*
