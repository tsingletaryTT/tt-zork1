# Models Directory

This directory will contain LLM models for inference-based parsing (Phase 3).

## Purpose

In Phase 3, we'll replace the classic rule-based text parser with LLM-based natural language understanding. This directory will store:

1. **Trained models** - Fine-tuned models for command parsing
2. **Model configurations** - Hyperparameters and settings
3. **Quantized models** - Optimized for Tenstorrent hardware
4. **Training data** - Command paraphrase datasets

## Model Requirements

### Size Constraints
- Must fit in Tenstorrent DRAM/L1 memory
- Target: < 1GB for model weights
- Quantization: INT8 or FP16

### Inference Latency
- Target: < 100ms per command
- Should not disrupt gameplay flow
- Pipeline parallelism to hide latency

### Architecture Candidates

1. **BERT/RoBERTa** (Encoder-only)
   - Fast inference
   - Good for classification tasks
   - Size: ~110M-350M parameters

2. **DistilBERT / TinyBERT**
   - Smaller, faster
   - Size: ~66M parameters
   - 97% of BERT accuracy

3. **Small decoder models**
   - TinyLlama (1.1B parameters)
   - Phi-2 (2.7B parameters)
   - More flexible but slower

## Training Data Structure

```
models/
├── training_data/
│   ├── commands.json          # Base Zork commands
│   ├── paraphrases.json       # Generated paraphrases
│   ├── context_examples.json  # Multi-turn examples
│   └── validation.json        # Test set
├── weights/
│   ├── parser-v1.bin          # Trained model weights
│   └── parser-v1-int8.bin     # Quantized model
└── configs/
    └── parser-config.json     # Model configuration
```

## Model Conversion Pipeline

1. **Train** on command dataset (likely PyTorch/HuggingFace)
2. **Export** to ONNX format
3. **Convert** to TT-NN format
4. **Quantize** for efficient inference
5. **Deploy** to Tenstorrent hardware

## TT-Metal Integration

Models will run as TT-Metal kernels on Tensix cores:
```
User Input → Tokenizer (CPU) → Model (Tensix) → Decoder (CPU) → Parsed Command
```

## Next Steps (Phase 3)

1. Extract Zork vocabulary from source files
2. Generate training data using GPT-4
3. Fine-tune small language model
4. Convert to TT-NN format
5. Implement inference kernels
6. Benchmark and optimize

---

**Current Status**: Phase 1 - Directory placeholder
**Large files**: Add *.bin, *.onnx, *.pt to .gitignore (already done)
