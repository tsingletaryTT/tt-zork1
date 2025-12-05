# Zork on Tenstorrent - Architecture Documentation

## Project Overview

This project ports Zork I to run on Tenstorrent Blackhole/Wormhole hardware with a hybrid architecture that eventually replaces the classic text parser with LLM-based inference.

## System Architecture

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────┐
│                  Tenstorrent Hardware                   │
├──────────────────────────┬──────────────────────────────┤
│   RISC-V Cores (16x)     │    Tensix++ Cores (140x)     │
│                          │                              │
│  ┌──────────────────┐    │   ┌─────────────────────┐   │
│  │   Z-Machine      │◄───┼──►│   LLM Inference     │   │
│  │   Interpreter    │    │   │   Engine            │   │
│  │   (Frotz-based)  │    │   │   (TT-NN)           │   │
│  └──────────────────┘    │   └─────────────────────┘   │
│         │                │            │                 │
│  ┌──────▼──────────┐     │   ┌────────▼────────────┐   │
│  │  Game State     │     │   │  Parser Inference   │   │
│  │  Memory         │     │   │  (replaces classic  │   │
│  │  Z-code Ops     │     │   │   parser logic)     │   │
│  └─────────────────┘     │   └─────────────────────┘   │
└──────────────────────────┴──────────────────────────────┘
```

### Component Breakdown

#### 1. Z-Machine Interpreter (RISC-V Cores)
**Location**: `src/zmachine/`
**Purpose**: Execute Z-machine bytecode (zork1.z3)
**Base**: Frotz interpreter core, stripped down to version 3 support

**Key Components**:
- **VM Core**: Instruction fetch, decode, execute cycle
- **Memory Manager**: Handles Z-machine memory model (dynamic, static, high memory)
- **Object System**: Game objects, properties, attributes
- **Stack Manager**: Call frames and expression evaluation
- **I/O Interface**: Text input/output abstraction

**Why RISC-V?**:
- General-purpose CPU suitable for sequential interpreter logic
- Straightforward port from existing C code
- Low-latency access for game state management

#### 2. Parser Abstraction Layer
**Location**: `src/parser/`
**Purpose**: Decouple text parsing from interpreter logic

**Key Components**:
- **Parser Interface**: Abstract API for pluggable parsers
- **Classic Parser**: Original Frotz/Z-machine parser (fallback)
- **Inference Parser**: LLM-based natural language parser (future)
- **Command Router**: Routes parsed commands to interpreter

**Command Schema** (based on Zork's gparser.zil):
```c
typedef struct {
    uint16_t verb;        // PRSA - verb action (e.g., TAKE, DROP, GO)
    uint16_t direct_obj;  // PRSO - primary object (e.g., LAMP, SWORD)
    uint16_t preposition; // Preposition (e.g., WITH, TO, FROM)
    uint16_t indirect_obj;// PRSI - secondary object (e.g., KNIFE, TROLL)
} ParsedCommand;
```

#### 3. I/O Abstraction Layer
**Location**: `src/io/`
**Purpose**: Platform-agnostic I/O for multiple environments

**Backends**:
- **Native**: Standard stdio for local testing (macOS/Linux)
- **TT-Metal**: Host communication via TT-Metal APIs
- **Debug**: Logging and tracing for development

**Interface**:
```c
// Print text to output
void io_print(const char* text);

// Read line of input from user
int io_read_line(char* buffer, size_t max_len);

// Check if input is available (non-blocking)
bool io_input_ready();
```

#### 4. LLM Inference Engine (Tensix Cores)
**Location**: `src/kernels/`
**Purpose**: Neural network inference for natural language parsing
**Framework**: TT-Metal/TT-NN

**Key Components**:
- **Tokenizer**: Convert text to model input tokens
- **Model Kernels**: Compute kernels implementing forward pass
- **Data Movement**: Circular buffers for RISC-V ↔ Tensix communication
- **Decoder**: Convert model output to ParsedCommand structure

**Why Tensix?**:
- Massively parallel compute ideal for matrix operations
- Optimized for AI/ML workloads
- Leverage purpose-built hardware for inference

## Development Workflow

### Local Development (macOS)
```
┌──────────────────────┐
│  Code Development    │
│  - Edit source       │
│  - Native builds     │
│  - Unit tests        │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  Local Testing       │
│  - Native x86/ARM    │
│  - Interpreter logic │
│  - Parser testing    │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  Git Commit & Push   │
└──────────────────────┘
```

### Hardware Deployment (Ubuntu 22.04 + Wormhole/Blackhole)
```
┌──────────────────────┐
│  Git Pull            │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  Build for RISC-V    │
│  - Cross-compile     │
│  - Link TT-Metal     │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  Deploy to Hardware  │
│  - Run on RISC-V     │
│  - Test on Tensix    │
│  - Benchmark         │
└──────────────────────┘
```

## Build System

### Dual Build Strategy

**Native Build** (`scripts/build_local.sh`):
- Compiles for host architecture (x86_64 / arm64)
- Links against standard C library
- Uses stdio for I/O
- Fast iteration for logic testing

**RISC-V Build** (`scripts/build_riscv.sh`):
- Cross-compiles for RISC-V target
- Links against TT-Metal SDK
- Uses TT-Metal host interface for I/O
- Deploys to Tenstorrent hardware

### Configuration System

Environment-specific configuration via build flags:
```c
#ifdef BUILD_NATIVE
  // Native stdio I/O
  #include <stdio.h>
#elif BUILD_TT_METAL
  // TT-Metal host interface
  #include "tt_metal/host_api.hpp"
#endif
```

## Memory Architecture

### Z-Machine Memory Model
- **Dynamic Memory**: 0x0000 - end of dynamic (modifiable during gameplay)
- **Static Memory**: After dynamic (read-only game data)
- **High Memory**: Routines and strings (packed format)

### Tenstorrent Memory Hierarchy
- **RISC-V L1**: Fast local memory for interpreter state
- **Shared L1**: Communication between RISC-V cores
- **DRAM**: Large storage for game state, model weights
- **Tensix L1**: Local memory for inference kernels

### Communication Strategy
**RISC-V → Tensix**:
1. Write input text to circular buffer in DRAM
2. Signal Tensix via kernel launch
3. Poll for completion or use callback

**Tensix → RISC-V**:
1. Inference kernel writes ParsedCommand to DRAM
2. Signal completion to RISC-V
3. RISC-V reads result and continues execution

## Development Phases

### Phase 1: Foundation (Current)
**Goal**: Z-machine interpreter running on RISC-V
**Status**: In Progress
**Deliverables**:
- [ ] Repository structure
- [ ] Build system (native + RISC-V)
- [ ] Frotz core integration
- [ ] Basic I/O abstraction
- [ ] Zork I playable on native build

### Phase 2: Hybrid Architecture
**Goal**: Parser abstraction and integration points
**Status**: Not Started
**Deliverables**:
- [ ] Parser abstraction layer
- [ ] Classic parser preserved
- [ ] Inference integration points designed
- [ ] Message passing infrastructure

### Phase 3: LLM Inference
**Goal**: Replace parser with LLM-based understanding
**Status**: Not Started
**Deliverables**:
- [ ] Model selection and quantization
- [ ] Training data from Zork commands
- [ ] TT-Metal inference kernels
- [ ] Parser integration with confidence scoring

### Phase 4: Optimization
**Goal**: Performance tuning and evaluation
**Status**: Not Started
**Deliverables**:
- [ ] Latency benchmarks
- [ ] Parser accuracy metrics
- [ ] Power/performance analysis
- [ ] Comparative study vs classic parser

## Testing Strategy

### Unit Tests
- Interpreter instruction execution
- Memory management operations
- Parser command validation
- I/O abstraction correctness

### Integration Tests
- Full gameplay scenarios
- Save/restore functionality
- Edge cases and error handling
- Cross-environment consistency (native vs hardware)

### Inference Tests (Phase 3)
- Command paraphrasing accuracy
- Context understanding
- Ambiguity resolution
- Fallback behavior

## Future Extensions

1. **Multi-modal Input**: Vision models for visual descriptions
2. **Dynamic World Generation**: LLM generates new content
3. **NPC Dialogue**: Inference-powered conversations
4. **Multi-language Support**: Real-time translation
5. **Accessibility Features**: Natural language accessibility

## References

- [Z-machine Specification](https://www.ifwiki.org/Z-machine)
- [Frotz Interpreter](https://gitlab.com/DavidGriffith/frotz)
- [TT-Metal Documentation](https://github.com/tenstorrent/tt-metal)
- [Tenstorrent Architecture](https://www.servethehome.com/tenstorrent-blackhole-and-metalium-for-standalone-ai-processing/)
- [LLM + Interactive Fiction](https://ianbicking.org/blog/2025/07/intra-llm-text-adventure)

---

**Last Updated**: 2025-12-04
**Status**: Phase 1 - In Progress
