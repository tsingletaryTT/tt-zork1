## Best practices
* Write well-documented, deeply commented code
* Review comments and docs and revise to match with reality often
* Always use a per-project CLAUDE.md file and describe "what happened?" succinctly. Save the original prompt and moments in prompting.
- Increment the extension version with changes like this. After I test something and come back with a bug.

## Project: Zork on Tenstorrent

**Goal**: Port Zork I to run on Tenstorrent AI accelerators using a hybrid architecture where RISC-V cores run the Z-machine interpreter and Tensix cores handle LLM-based natural language parsing.

### Original Prompt (2025-12-04)
"This is Zork. It runs on the ZMachine. It's historical. I want to run Zmachine on "pure metal" of Tenstorrent. Build a plan to take this code base and make it run on a Tenstorrent Blackhole or Wormhole card."

**Key Insight from User**: The goal is to eventually replace the classic text parser with LLM inference running on Tensix cores, creating a novel interactive fiction experience.

### Development Log

#### Phase 1.1: Repository Setup (Complete ✅)
- Created directory structure for multi-environment deployment
- Set up dual build system (native + RISC-V cross-compile)
- Created comprehensive documentation
- Established git workflow: local dev → commit/push → pull on hardware

#### Phase 1.2: Frotz Integration (Complete ✅ - Jan 12, 2026)
**What Happened:**
- Successfully integrated Frotz 2.56pre Z-machine interpreter
- Implemented complete I/O abstraction layer for platform independence
- Implemented all 40+ Frotz OS interface functions
- Build system compiles Frotz core + our integration seamlessly
- Z-machine initializes and runs perfectly
- **Full gameplay tested and working** - opening mailbox, reading leaflets, navigation all functional

**Technical Wins:**
- Clean separation: interpreter core / I/O layer / platform
- Proper initialization sequence discovered through debugging
- All global variables and symbols properly resolved
- Binary builds cleanly with no errors
- Text encoding issue resolved via _DEFAULT_SOURCE flag (fixed strdup/strndup declarations)

**Files Created:**
- `src/io/io.h` - I/O abstraction API (~90 lines)
- `src/io/io_native.c` - Native implementation (~120 lines)
- `src/zmachine/frotz_os.c` - OS interface (~400 lines)
- `src/zmachine/main.c` - Entry point (~105 lines)
- `scripts/build_local.sh` - Build system (updated)
- `docs/PHASE_1_2_COMPLETE.md` - Complete status report

**Key Learnings:**
1. Frotz initialization order is critical - init_memory() MUST come before os_init_screen()
2. Object file naming conflicts required prefix strategy
3. Frotz's main.c must be excluded, we provide our own
4. Z-machine header (z_header) configuration is essential
5. All globals must be defined even if unused (linker requirement)
6. _DEFAULT_SOURCE flag critical for proper string function declarations on Linux

**Resolution:**
Previous text encoding issue was resolved by adding _DEFAULT_SOURCE to CFLAGS, which properly declares strdup/strndup. Without this, these functions returned int instead of char*, causing segfaults and text corruption.

### Build Commands

```bash
# Native build (macOS/Linux)
./scripts/build_local.sh [clean|debug|release]

# Run Zork
./zork-native game/zork1.z3

# RISC-V build (for hardware)
./scripts/build_riscv.sh release

# Deploy to hardware
./scripts/deploy.sh [wormhole|blackhole]
```

### Architecture Notes

**Current**: RISC-V interpreter + Native I/O
**Phase 2**: Add parser abstraction layer
**Phase 3**: Integrate LLM inference on Tensix cores
**End Goal**: Hybrid text adventure with AI-powered natural language understanding

#### Phase 1.3: RISC-V Cross-compilation (Complete ✅ - Jan 12, 2026)
**Goal**: Cross-compile the working native build to RISC-V architecture for Tenstorrent hardware.

**What Happened:**
- Installed RISC-V GNU toolchain (riscv64-unknown-elf-gcc) via Homebrew
- Successfully cross-compiled Zork interpreter for RISC-V 64-bit architecture
- Binary: 192KB statically linked executable
- Architecture: RV64GC (64-bit with compressed instructions, double-float ABI)
- Build produces: ELF 64-bit LSB executable for UCB RISC-V

**Technical Challenges Solved:**
1. **Missing POSIX functions**: bare-metal toolchain lacks `basename()` and `strrchr()`
   - Solution: Enabled Frotz's built-in compatibility functions via `-DNO_BASENAME -DNO_STRRCHR`
2. **Missing string.h**: Frotz's compatibility functions need standard string operations
   - Solution: Force-include string.h with `-include string.h` flag
3. **Toolchain selection**: Multiple RISC-V toolchains available, needed one with newlib
   - Solution: riscv-software-src/riscv tap provides toolchain with proper newlib support

**Binary Details:**
- Size: 174KB text + 9KB data + 8KB BSS = 192KB total
- Entry point: 0x10150
- Statically linked (ready for bare-metal deployment)
- Includes debug symbols (can be stripped for production)

#### Phase 2: LLM-Backed Natural Language Parser (Complete ✅ - Jan 12, 2026)
**Goal**: Integrate an LLM to translate natural language into Zork commands, making the game accessible via conversational input.

**What Happened:**
- Built complete LLM translation system with 10 new modules (~3000 lines of heavily-documented code)
- Integrated with Frotz's input/output system
- Prompts stored in editable text files (educational focus)
- Full conversation history captured for context
- Graceful fallback to standard parser if LLM unavailable

**Architecture:**
```
User: "I want to open the mailbox"
  ↓
[Capture game history] → Context Manager
  ↓
[Load prompts] → Prompt Loader (from prompts/*.txt)
  ↓
[Format request] → JSON Builder
  ↓
[API call] → LLM Client (libcurl → localhost:1234)
  ↓
LLM Response: "open mailbox"
  ↓
[Display translation]
  ↓
Z-machine executes → Game continues
```

**Modules Implemented:**
1. **Prompt Loader** (`src/llm/prompt_loader.{c,h}`) - Loads prompts from text files, not hardcoded!
2. **Context Manager** (`src/llm/context.{c,h}`) - Circular buffer storing last 20 turns
3. **Output Capture** (`src/llm/output_capture.{c,h}`) - Hooks into Frotz display functions
4. **JSON Helper** (`src/llm/json_helper.{c,h}`) - Minimal JSON for OpenAI API (no heavy deps)
5. **LLM Client** (`src/llm/llm_client.{c,h}`) - HTTP client using libcurl
6. **Input Translator** (`src/llm/input_translator.{c,h}`) - Main orchestrator
7. **Prompts** (`prompts/`) - User-editable prompt files + educational README

**Educational Features:**
- Every file has 100+ lines of explanatory comments
- Prompts in separate `.txt` files - no recompilation to iterate!
- `prompts/README.md` teaches prompt engineering
- Statistics tracking (success rate, fallbacks)
- All errors non-fatal - game always playable

**Integration Points:**
- `dinput.c:os_read_line()` - Intercepts user input for translation
- `doutput.c:os_display_string()` - Captures game output for context
- `dinit.c:os_init_screen()` - Initializes LLM system
- `dinit.c:os_quit()` - Shutdown and statistics

**Configuration (Environment Variables):**
- `ZORK_LLM_MOCK` - Set to "1" for mock mode (first 4 screens, no server needed)
- `ZORK_LLM_URL` - API endpoint (default: http://localhost:1234/v1/chat/completions)
- `ZORK_LLM_MODEL` - Model name (default: "zork-assistant")
- `ZORK_LLM_API_KEY` - Optional for remote APIs
- `ZORK_LLM_ENABLED` - Set to "0" to disable

**Testing:**
```bash
# Option 1: Mock mode (no LLM server needed - first 4 screens only)
export ZORK_LLM_MOCK=1
./zork-native game/zork1.z3

# Option 2: Real LLM (start local LLM server like LM Studio on port 1234)
export ZORK_LLM_URL="http://localhost:1234/v1/chat/completions"
export ZORK_LLM_MODEL="your-model-name"
./zork-native game/zork1.z3

# Try natural language:
> I want to open the mailbox
[LLM → open mailbox]
Opening the small mailbox reveals a leaflet.

> Please pick up the leaflet and read it
[LLM → take leaflet, read leaflet]
Taken. "WELCOME TO ZORK! ..."

# Statistics displayed on exit:
=== Translation Statistics ===
Total attempts:  4
Successful:      4
Fallbacks:       0
Success rate:    100%
```

**Future-Ready Design:**
- Modular architecture supports LLM-to-LLM gameplay (autonomous agents)
- Native-only for now, but structure supports future RISC-V port
- Could swap HTTP client for TT-Metal host communication

**Files Modified:**
- `src/zmachine/frotz/src/dumb/dinput.c` - Added translation call
- `src/zmachine/frotz/src/dumb/doutput.c` - Added output capture
- `src/zmachine/frotz/src/dumb/dinit.c` - Added init/shutdown
- `scripts/build_local.sh` - Added libcurl detection and LLM compilation

**Dependencies Added:**
- libcurl (for HTTP API calls)

### Project Status

- [x] Phase 1.1: Repository structure and build system
- [x] Phase 1.2: Frotz integration - **FULLY FUNCTIONAL**
- [x] Phase 1.3: RISC-V cross-compilation - **COMPLETE**
- [x] Phase 2: LLM Natural Language Parser - **COMPLETE** ✅
  - 10 modules implemented (~3000 lines)
  - Mock mode for testing without LLM server
  - Full conversation history context
  - Graceful fallback on errors
- [ ] Phase 3: Hardware deployment and testing (Ready to start)
- [ ] Phase 4: Tensix inference integration (Future)
- [ ] Phase 5: Optimization and benchmarking

**Current Milestone**: LLM natural language interface working! Can play Zork using conversational input like "I want to open the mailbox". Native build complete, RISC-V ready for hardware deployment.

### Hardware Access

**Development**: macOS (local, no hardware)
**Deployment**: Wormhole + Blackhole cards (cloud/hardware environments)
**Workflow**: Develop locally → git push → pull and test on hardware
