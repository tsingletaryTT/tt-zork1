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

**Next Steps:**
- Deploy to Tenstorrent hardware with RISC-V cores
- Test execution in TT-Metal environment
- Verify Z-machine runs correctly on hardware

### Project Status

- [x] Phase 1.1: Repository structure and build system
- [x] Phase 1.2: Frotz integration - **FULLY FUNCTIONAL**
- [x] Phase 1.3: RISC-V cross-compilation - **COMPLETE**
- [ ] Phase 1.4: Hardware deployment and testing (Ready to start)
- [ ] Phase 2: Parser abstraction layer
- [ ] Phase 3: LLM inference integration
- [ ] Phase 4: Optimization and benchmarking

**Current Milestone**: RISC-V binary ready for hardware deployment! Native and cross-compiled builds both functional.

### Hardware Access

**Development**: macOS (local, no hardware)
**Deployment**: Wormhole + Blackhole cards (cloud/hardware environments)
**Workflow**: Develop locally → git push → pull and test on hardware
