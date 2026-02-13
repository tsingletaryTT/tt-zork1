# Directory Cleanup and Organization Plan

**Date**: February 13, 2026
**Status**: Proposed cleanup to preserve historic moments while improving clarity

---

## Current Issues

The repository has accumulated many files over time:
- **53 root-level files** (should be ~5-10)
- **40+ kernel test files** in `kernels/` (many are historic experiments)
- **13+ test program files** in root directory
- **Multiple markdown milestone documents** that could be organized
- **Build artifacts** mixed with source

---

## Proposed Structure

```
tt-zork1/
├── README.md                          # Main project overview
├── CLAUDE.md                          # Project instructions
├── LICENSE
├── QUICK_START.md                     # Get started in 5 min
│
├── config/                            # Configuration files
│   ├── llm_mode.yaml                  # Current LLM mode
│   └── llm_mode.yaml.example          # Example configs
│
├── docs/                              # All documentation
│   ├── architecture/                  # Architecture docs
│   │   ├── FLEXIBLE_ARCHITECTURE.md
│   │   ├── FOUR_CHIP_ARCHITECTURE.md
│   │   └── MULTI_LLM_ARCHITECTURE.md
│   ├── implementation/                # Implementation guides
│   │   ├── LLM_ROUTER_GUIDE.md
│   │   ├── LLM_SUPPORT.md
│   │   ├── PROMPT_ENGINEERING_GUIDE.md
│   │   ├── READ_OPCODE_IMPLEMENTATION.md
│   │   └── STATIC_COMPILATION_OPTIONS.md
│   ├── hardware/                      # Hardware-specific docs
│   │   ├── BLACKHOLE_RISCV.md
│   │   ├── DEVICE_RECOVERY.md
│   │   └── TENSTORRENT_EXPLAINED.md
│   ├── milestones/                    # Historic achievements
│   │   ├── 2026-01-18-BLACKHOLE_DICTIONARY_DECODED.md
│   │   ├── 2026-01-20-FIRST_BLACKHOLE_RUN.md
│   │   ├── 2026-01-20-FIRST_ZORK_TEXT_ON_BLACKHOLE.md
│   │   ├── 2026-01-20-ZMACHINE_EXECUTION.md
│   │   ├── 2026-01-21-HELLO_FROM_RISC-V.md
│   │   ├── 2026-01-22-KERNEL_COMPILED.md
│   │   ├── 2026-01-22-ZMACHINE_HEADER_READ.md
│   │   └── 2026-01-28-BREAKTHROUGH_READ_OPCODE.md
│   ├── sessions/                      # Session summaries
│   │   ├── 2026-01-16-SESSION_SUMMARY.md
│   │   ├── 2026-01-17-SESSION_SUMMARY.md
│   │   ├── 2026-01-18-BREAKTHROUGH.md
│   │   └── 2026-01-18-SESSION_SUMMARY_PART2.md
│   └── status/                        # Status tracking
│       ├── BUILD_STATUS.md
│       ├── IMPLEMENTATION_STATUS.md
│       ├── IMPLEMENTATION_SUMMARY.md
│       ├── JOURNEY_MAPPING_SUMMARY.md
│       └── TESTING_STATUS.md
│
├── game/                              # Game files
│   └── zork1.z3                       # Original Zork I game file
│
├── src/                               # All source code
│   ├── io/                            # I/O abstraction layer
│   ├── journey/                       # Journey mapping system
│   ├── llm/                           # LLM integration
│   │   ├── llm_client.c/h
│   │   ├── llm_router.c/h            # NEW: Router layer
│   │   ├── prompt_loader.c/h
│   │   ├── context.c/h
│   │   ├── json_helper.c/h
│   │   └── input_translator.c/h
│   └── zmachine/                      # Z-machine interpreter
│       ├── frotz/                     # Frotz submodule
│       └── main.c
│
├── kernels/                           # TT-Metal RISC-V kernels
│   ├── README.md                      # Index of kernels
│   ├── active/                        # Currently used kernels
│   │   ├── zork_interpreter_opt.cpp  # Optimized interpreter
│   │   └── hello_riscv.cpp           # Hello world test
│   └── archive/                       # Historic experiments (39 files)
│       ├── exploration/               # Early exploration (2026-01)
│       │   ├── test_hello.cpp
│       │   ├── test_minimal.cpp
│       │   ├── zork_scan_print.cpp
│       │   └── ...
│       ├── decoding/                  # Object/string decoding (2026-01)
│       │   ├── zork_decode_objects.cpp
│       │   ├── zork_objects_with_abbrev.cpp
│       │   └── ...
│       └── interpreters/              # Interpreter iterations
│           ├── zork_interpreter_v1.cpp
│           ├── zork_monolithic.cpp
│           └── ...
│
├── tests/                             # Test suite
│   ├── README.md
│   ├── run_tests.sh
│   ├── unit/                          # Unit tests
│   │   ├── test_tracker.c
│   │   ├── test_game_state.c
│   │   └── test_room_abbreviation.c
│   └── integration/                   # Integration tests
│       ├── test_router.c              # NEW: Router tests
│       ├── test_map_demo.c
│       └── test_map_visual.c
│
├── host_programs/                     # TT-Metal host programs (C++)
│   ├── README.md                      # Index of programs
│   ├── zork_on_blackhole.cpp         # Main Zork-on-hardware program
│   ├── zork_batched.cpp              # Batched execution
│   ├── zork_repl.cpp                 # REPL interface
│   └── archive/                       # Historic test programs (14 files)
│       ├── test_device_init.cpp
│       ├── test_hello_kernel.cpp
│       ├── test_buffers.cpp
│       └── ...
│
├── scripts/                           # Build and utility scripts
│   ├── build/                         # Build scripts
│   │   ├── build_local.sh
│   │   ├── build_riscv.sh
│   │   ├── build_blackhole_kernel.sh
│   │   └── deploy.sh
│   ├── llm/                           # LLM management
│   │   ├── start-four-llms.sh
│   │   ├── stop-four-llms.sh
│   │   ├── check-server-health.sh
│   │   ├── switch-mode.sh            # NEW
│   │   └── get-model-names.sh
│   ├── testing/                       # Testing scripts
│   │   ├── test-four-llms.sh
│   │   ├── test-llm-router.sh        # NEW
│   │   ├── test-prompt-variants.sh
│   │   ├── compare-prompts.sh
│   │   └── quick-test-translator.sh
│   ├── gameplay/                      # Gameplay scripts
│   │   ├── play-zork-interactive.sh
│   │   ├── play-zork-hardware.sh
│   │   ├── run-zork-llm.sh
│   │   └── demo-*.sh
│   └── utils/                         # Utilities
│       ├── tt-cold-reboot.sh
│       └── extract-command.sh
│
├── prompts/                           # LLM prompts
│   ├── README.md
│   ├── config.yaml
│   ├── translator/
│   ├── artist/
│   ├── dm/
│   └── player/
│
├── archive/                           # OLD: Historic artifacts
│   ├── zil/                           # Original Zork ZIL source
│   │   ├── zork1.zil
│   │   ├── 1actions.zil
│   │   └── ...
│   └── builds/                        # Old build artifacts
│       └── COMPILED/                  # Compiled game files
│
├── build-native/                      # Build directory (gitignored)
├── build-host/                        # TT-Metal build (gitignored)
├── .gitignore
└── CMakeLists.txt
```

---

## Cleanup Actions

### Phase 1: Create New Directories ✅

```bash
# Create organized structure
mkdir -p docs/{architecture,implementation,hardware,milestones,sessions,status}
mkdir -p kernels/{active,archive/{exploration,decoding,interpreters}}
mkdir -p host_programs/archive
mkdir -p tests/integration
mkdir -p scripts/{build,llm,testing,gameplay,utils}
mkdir -p archive/{zil,builds}
```

### Phase 2: Move Documentation

```bash
# Move architecture docs
mv docs/FLEXIBLE_ARCHITECTURE.md docs/architecture/
mv docs/FOUR_CHIP_ARCHITECTURE.md docs/architecture/
mv docs/MULTI_LLM_ARCHITECTURE.md docs/architecture/

# Move implementation docs
mv docs/LLM_ROUTER_GUIDE.md docs/implementation/
mv docs/LLM_SUPPORT.md docs/implementation/
mv docs/PROMPT_ENGINEERING_GUIDE.md docs/implementation/
mv docs/READ_OPCODE_IMPLEMENTATION.md docs/implementation/
mv docs/STATIC_COMPILATION_OPTIONS.md docs/implementation/

# Move hardware docs
mv docs/BLACKHOLE_RISCV.md docs/hardware/
mv docs/DEVICE_RECOVERY.md docs/hardware/
mv docs/TENSTORRENT_EXPLAINED.md docs/hardware/

# Move milestone docs (add date prefixes)
mv BLACKHOLE_DICTIONARY_DECODED.md docs/milestones/2026-01-18-BLACKHOLE_DICTIONARY_DECODED.md
mv FIRST_BLACKHOLE_RUN.md docs/milestones/2026-01-20-FIRST_BLACKHOLE_RUN.md
mv FIRST_ZORK_TEXT_ON_BLACKHOLE.md docs/milestones/2026-01-20-FIRST_ZORK_TEXT_ON_BLACKHOLE.md
mv MILESTONE_ZMACHINE_EXECUTION.md docs/milestones/2026-01-20-ZMACHINE_EXECUTION.md
mv MILESTONE_HELLO_FROM_RISC-V.md docs/milestones/2026-01-21-HELLO_FROM_RISC-V.md
mv MILESTONE_KERNEL_COMPILED.md docs/milestones/2026-01-22-KERNEL_COMPILED.md
mv MILESTONE_ZMACHINE_HEADER_READ.md docs/milestones/2026-01-22-ZMACHINE_HEADER_READ.md

# Move session summaries
mv SESSION_SUMMARY_JAN16-17.md docs/sessions/2026-01-16-SESSION_SUMMARY.md
mv SESSION_SUMMARY_JAN18_2026_PART2.md docs/sessions/2026-01-18-SESSION_SUMMARY_PART2.md
mv BREAKTHROUGH_JAN18_2026.md docs/sessions/2026-01-18-BREAKTHROUGH.md

# Move status docs
mv BUILD_STATUS.md docs/status/
mv IMPLEMENTATION_STATUS.md docs/status/
mv IMPLEMENTATION_SUMMARY.md docs/status/
mv JOURNEY_MAPPING_SUMMARY.md docs/status/
mv TESTING_STATUS.md docs/status/
```

### Phase 3: Organize Kernels

```bash
# Move active kernels
mv kernels/zork_interpreter_opt.cpp kernels/active/
mv kernels/hello_riscv.cpp kernels/active/

# Move exploration kernels to archive
mv kernels/test_*.cpp kernels/archive/exploration/
mv kernels/zork_scan_*.cpp kernels/archive/exploration/
mv kernels/zork_find_*.cpp kernels/archive/exploration/
mv kernels/zork_opening_*.cpp kernels/archive/exploration/
mv kernels/zork_high_memory_scan.cpp kernels/archive/exploration/

# Move decoding kernels to archive
mv kernels/zork_decode_*.cpp kernels/archive/decoding/
mv kernels/zork_object*.cpp kernels/archive/decoding/
mv kernels/zork_dict*.cpp kernels/archive/decoding/
mv kernels/zork_frotz_decode.cpp kernels/archive/decoding/

# Move interpreter iterations to archive
mv kernels/zork_interpreter_v1.cpp kernels/archive/interpreters/
mv kernels/zork_interpreter.cpp kernels/archive/interpreters/
mv kernels/zork_monolithic.cpp kernels/archive/interpreters/
mv kernels/zork_execute*.cpp kernels/archive/interpreters/
mv kernels/zork_kernel.cpp kernels/archive/interpreters/
mv kernels/zork_minimal_executor.cpp kernels/archive/interpreters/
mv kernels/zork_frotz_*.cpp kernels/archive/interpreters/

# Create kernel index
cat > kernels/README.md << 'EOF'
# TT-Metal RISC-V Kernels

## Active Kernels

- **zork_interpreter_opt.cpp** - Optimized Z-machine interpreter (CURRENT)
- **hello_riscv.cpp** - Basic "Hello World" test kernel

## Archive

Historic kernel experiments organized by purpose:
- `archive/exploration/` - Early Z-machine exploration (Jan 18-20, 2026)
- `archive/decoding/` - Object and string decoding tests (Jan 18-20, 2026)
- `archive/interpreters/` - Interpreter evolution (Jan 20-28, 2026)

See milestone docs for context on each phase.
EOF
```

### Phase 4: Organize Host Programs

```bash
# Move test programs to archive
mv test_*.cpp host_programs/archive/

# Move main programs to host_programs
mv zork_on_blackhole.cpp host_programs/
mv zork_batched.cpp host_programs/
mv zork_on_blackhole_streaming.cpp host_programs/archive/

# Create host programs index
cat > host_programs/README.md << 'EOF'
# TT-Metal Host Programs

## Main Programs

- **zork_on_blackhole.cpp** - Main Zork-on-hardware program
- **zork_batched.cpp** - Batched execution (state persistence)

## Archive

Historic test programs from TT-Metal bring-up (Jan 21-22, 2026).
See docs/milestones/ for context.
EOF
```

### Phase 5: Organize Tests

```bash
# Move router test to integration
mv test_router.c tests/integration/

# Move visual map tests
mv test-map-visual tests/integration/
mv test-map-visual.c tests/integration/
```

### Phase 6: Organize Scripts

```bash
# Build scripts
mv scripts/build_*.sh scripts/build/
mv scripts/deploy.sh scripts/build/

# LLM scripts
mv scripts/*llm*.sh scripts/llm/
mv scripts/check-server-health.sh scripts/llm/
mv scripts/switch-mode.sh scripts/llm/
mv scripts/get-model-names.sh scripts/llm/

# Testing scripts
mv scripts/test-*.sh scripts/testing/
mv scripts/compare-prompts.sh scripts/testing/
mv scripts/quick-test-translator.sh scripts/testing/
mv scripts/tune-prompt-interactive.sh scripts/testing/

# Gameplay scripts
mv play-*.sh scripts/gameplay/
mv run-zork-*.sh scripts/gameplay/
mv demo-*.sh scripts/gameplay/
mv test-map-demo.sh scripts/gameplay/

# Utilities
mv scripts/tt-cold-reboot.sh scripts/utils/
mv scripts/extract-command.sh scripts/utils/
```

### Phase 7: Archive ZIL Source

```bash
# Move ZIL source files
mv *.zil archive/zil/
mv zork1.* archive/zil/
mv parser.cmp archive/zil/

# Move compiled artifacts
mv COMPILED archive/builds/
```

### Phase 8: Clean Root Directory

```bash
# Remove or archive temporary files
rm -f wget-log
rm -f parser.cmp

# Move remaining docs
mv TODAY_EPIC_JOURNEY.md docs/sessions/2026-01-20-EPIC_JOURNEY.md
mv RESUME_NEXT_SESSION.md docs/status/
mv SESSION_PROGRESS.md docs/status/
mv BLACKHOLE_FIRST_RUN_RESULTS.md docs/milestones/2026-01-20-FIRST_RUN_RESULTS.md
mv HARDWARE_FAULT_REPORT.md docs/hardware/2026-01-21-HARDWARE_FAULT_REPORT.md
```

---

## Update Paths

After reorganization, update references in:

1. **README.md** - Update all doc links
2. **QUICK_START.md** - Update script paths
3. **CMakeLists.txt** - Update source paths if needed
4. **Build scripts** - Update relative paths
5. **.gitignore** - Ensure build dirs ignored

---

## Benefits

### Before:
- 53 root-level files
- Unclear which files are historic vs current
- Mixed purposes (docs, tests, sources, artifacts)
- Hard to find relevant files

### After:
- ~10 root-level files (README, LICENSE, config, etc.)
- Clear separation: current vs historic
- Organized by purpose (docs/src/tests/scripts)
- Easy navigation and onboarding

---

## Historic Preservation

All historic files are PRESERVED in `archive/` or `docs/milestones/`:
- ✅ Every ZIL source file
- ✅ Every milestone document (with dates)
- ✅ Every kernel experiment (with context)
- ✅ Every test program (with README index)
- ✅ Session summaries and breakthrough moments

Nothing is deleted - just organized!

---

## Next Steps

1. Review this plan
2. Create all new directories
3. Execute moves in phases
4. Update documentation references
5. Test builds still work
6. Commit changes with clear message:
   ```
   refactor: Reorganize directory structure for clarity

   - Move 40+ docs into categorized subdirectories
   - Archive 39 historic kernels with context
   - Organize scripts by purpose
   - Preserve all historic artifacts
   - No functionality changes, pure organization
   ```

---

**Result**: A clean, professional repository structure that preserves history while improving discoverability! 🎯
