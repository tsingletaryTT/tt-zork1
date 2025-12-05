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

#### Phase 1.2: Frotz Integration (Complete ✅ - Dec 4, 2025)
**What Happened:**
- Successfully integrated Frotz 2.56pre Z-machine interpreter
- Implemented complete I/O abstraction layer for platform independence
- Implemented all 40+ Frotz OS interface functions
- Build system compiles Frotz core + our integration seamlessly
- Z-machine initializes and runs

**Technical Wins:**
- Clean separation: interpreter core / I/O layer / platform
- Proper initialization sequence discovered through debugging
- All global variables and symbols properly resolved
- Binary builds cleanly with no errors

**Current Issue (In Progress 🔧):**
- Character output is garbled - text appears as "krAtmtt drs lea bet-03 Ti snral aa" instead of readable English
- "Illegal object" error suggests initialization issue
- Root cause: Character encoding/decoding path needs debugging
- Severity: High (blocks gameplay) but solvable

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

**Next Steps:**
1. Debug character output encoding issue
2. Fix "Illegal object" error
3. Verify complete gameplay works
4. Commit working version
5. Begin RISC-V cross-compilation (Phase 1.3)

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

### Debugging Notes (2025-12-04)

**Text Encoding Issue:**
- Symptom: Output shows "krAtmtt drs lea bet-03 Ti snral aa"
- Expected: "ZORK I: The Great Underground Empire..."
- Working reference: dfrotz in src/zmachine/frotz/ produces correct output
- Hypothesis: Character translation table or decode path issue
- Tools: Compare with dfrotz using debugger, check translate_from_zscii()

**Areas to Investigate:**
1. os_display_char() / os_display_string() implementation
2. Character set initialization in text.c
3. Z-character to Unicode/UTF-8 conversion
4. Screen buffer state during output
5. Compare memory dumps: our version vs working dfrotz

### Project Status

- [x] Phase 1.1: Repository structure and build system
- [x] Phase 1.2: Frotz integration (core complete, debugging text output)
- [ ] Phase 1.3: RISC-V cross-compilation
- [ ] Phase 1.4: Hardware deployment and testing
- [ ] Phase 2: Parser abstraction layer
- [ ] Phase 3: LLM inference integration
- [ ] Phase 4: Optimization and benchmarking

**Milestone**: First working build achieved! Z-machine runs, just needs text output fix.

### Hardware Access

**Development**: macOS (local, no hardware)
**Deployment**: Wormhole + Blackhole cards (cloud/hardware environments)
**Workflow**: Develop locally → git push → pull and test on hardware
