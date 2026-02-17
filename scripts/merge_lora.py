#!/usr/bin/env python3
"""
merge_lora.py - Merge LoRA adapter weights into base model

Usage:
    python scripts/merge_lora.py <base_model> <lora_adapter> <output_dir>

Example:
    python scripts/merge_lora.py \
        meta-llama/Llama-3.2-1B-Instruct \
        AvaLovelace/LLaMA-ASCII-Art \
        ./models/Llama-3.2-1B-ASCII-merged

What this does:
    1. Loads the base model (e.g., Llama-3.2-1B-Instruct)
    2. Loads the LoRA adapter on top (e.g., ASCII art fine-tune)
    3. Merges the adapter weights into the base model
    4. Saves as a standalone checkpoint (no LoRA dependency)
    5. Result can be loaded as a normal model

Requirements:
    pip install transformers peft torch

Technical details:
    - Uses PEFT's merge_and_unload() method
    - Creates a standard HuggingFace checkpoint
    - Output can be used with any library (vLLM, transformers, etc.)
    - Memory requirement: ~2x model size during merge
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

    # Check if PEFT is available
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

    # Step 3: Merge adapter weights into base model
    print(f"\n[3/5] Merging LoRA weights into base model...")
    try:
        merged_model = model.merge_and_unload()
        print(f"  ✓ Successfully merged adapter weights")
    except Exception as e:
        print(f"  ✗ Merge failed: {e}")
        sys.exit(1)

    # Step 4: Save merged model
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

    # Success summary
    print()
    print("=" * 70)
    print("  ✅ SUCCESS! LoRA adapter merged into base model")
    print("=" * 70)
    print()
    print(f"Output location: {output_dir}")
    print()
    print("You can now use this model with:")
    print(f"  tt serve start {output_dir} --port 8001")
    print()
    print("Or load in Python:")
    print(f"  from transformers import AutoModelForCausalLM")
    print(f"  model = AutoModelForCausalLM.from_pretrained('{output_dir}')")
    print()

if __name__ == "__main__":
    main()
