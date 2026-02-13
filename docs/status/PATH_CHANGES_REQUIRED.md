# Path Changes Required After Reorganization

**Status**: Analysis of what needs to be updated after running `scripts/reorganize-repo.sh`

---

## Summary

The reorganization moves files but **does NOT automatically update references**. This document lists all code changes needed after reorganization.

**Good News**: Most source code paths stay the same! The main changes are:
1. Script-to-script references (14 files)
2. Hardcoded kernel paths in host programs (8 files)
3. Documentation links (many files)

---

## Category 1: Host Programs (C++) - CRITICAL ⚠️

These programs have hardcoded absolute paths to kernels that will move.

### Files to Update

**`zork_on_blackhole.cpp`** (1 change):
```cpp
// OLD:
"/home/ttuser/code/tt-zork1/kernels/zork_interpreter_opt.cpp"

// NEW:
"/home/ttuser/code/tt-zork1/kernels/active/zork_interpreter_opt.cpp"
```

**`test_hello_kernel.cpp`** (1 change):
```cpp
// OLD:
"/home/ttuser/tt-zork1/kernels/hello_riscv.cpp"

// NEW:
"/home/ttuser/tt-zork1/kernels/active/hello_riscv.cpp"
```

**`test_zork_optimized.cpp`** (1 change):
```cpp
// OLD:
"/home/ttuser/code/tt-zork1/kernels/zork_interpreter_opt.cpp"

// NEW:
"/home/ttuser/code/tt-zork1/kernels/active/zork_interpreter_opt.cpp"
```

### Archive Programs (Can Update Later)

These are moving to `host_programs/archive/` - update if needed:
- `zork_batched.cpp` - references `kernels/zork_interpreter.cpp`
- `zork_on_blackhole_streaming.cpp` - references `kernels/zork_interpreter.cpp`
- `test_pinned_hostside.cpp` - references `kernels/test_pinned_output.cpp`
- `test_cached_execution.cpp` - references `kernels/noop_riscv.cpp`
- `test_pinned_memory.cpp` - references `kernels/test_pinned_write.cpp`
- `test_state_simple.cpp` - references `kernels/test_simple_state.cpp`

---

## Category 2: Scripts Calling Other Scripts

### Build Scripts

**`scripts/deploy.sh`** → **`scripts/build/deploy.sh`**:
```bash
# OLD:
./scripts/build_riscv.sh release

# NEW:
./scripts/build/build_riscv.sh release
```

### Testing Scripts

**`scripts/test-llm-router.sh`** → **`scripts/testing/test-llm-router.sh`**:
```bash
# OLD:
./scripts/switch-mode.sh "$MODE"
./scripts/check-server-health.sh
./scripts/quick-test-translator.sh "$input"

# NEW:
./scripts/llm/switch-mode.sh "$MODE"
./scripts/llm/check-server-health.sh
./scripts/testing/quick-test-translator.sh "$input"
```

**`scripts/start-four-llms.sh`** → **`scripts/llm/start-four-llms.sh`**:
```bash
# OLD (in output messages):
./scripts/check-server-health.sh
./scripts/test-four-llms.sh
./scripts/stop-four-llms.sh

# NEW:
./scripts/llm/check-server-health.sh
./scripts/testing/test-four-llms.sh
./scripts/llm/stop-four-llms.sh
```

**Similar updates needed in**:
- `scripts/tune-prompt-interactive.sh`
- `scripts/check-server-health.sh`

---

## Category 3: Root-Level Scripts (Moving to scripts/)

These scripts will move and need to update their own references:

**`play-zork-interactive.sh`** → **`scripts/gameplay/play-zork-interactive.sh`**:
- May reference `./zork-native` or `game/zork1.z3`
- Need to adjust relative paths

**`run-zork-llm.sh`** → **`scripts/gameplay/run-zork-llm.sh`**:
- May reference `./scripts/start-four-llms.sh`
- Needs: `../llm/start-four-llms.sh`

**`demo-*.sh`** → **`scripts/gameplay/demo-*.sh`**:
- Check for references to binaries or scripts

---

## Category 4: Documentation Links

Many docs have links to other docs or files. These need updates:

### Main README

**`README.md`**:
- Links to docs (now in subdirectories)
- Links to scripts (now in subdirectories)
- Links to prompts (stay same - good!)

### Documentation Files

**Files with many internal links**:
- `docs/DOCUMENTATION_SUMMARY.md` - Index of all docs
- `docs/FLEXIBLE_ARCHITECTURE.md` - References scripts
- `docs/FOUR_CHIP_ARCHITECTURE.md` - References scripts
- `docs/LLM_ROUTER_GUIDE.md` - References scripts and config
- `docs/PROMPT_ENGINEERING_GUIDE.md` - References scripts

**Pattern to update**:
```markdown
<!-- OLD -->
See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
Run `./scripts/start-four-llms.sh`

<!-- NEW -->
See [Architecture Docs](docs/architecture/ARCHITECTURE.md)
Run `./scripts/llm/start-four-llms.sh`
```

---

## Category 5: Build System

### Good News ✅

The build system should **mostly work** without changes:

**What stays the same**:
- `src/llm/*.c` - No change
- `src/io/*.c` - No change
- `src/journey/*.c` - No change
- `src/zmachine/` - No change
- `prompts/` - No change
- `game/` - No change
- `config/` - No change

**What might need updates**:
- None! Build scripts reference source directories that aren't changing

---

## Smart Reorganization Strategy

To minimize breakage, we can reorganize in phases:

### Phase 1: Safe Moves (No Code Changes)
Move files that have NO hardcoded references:
- Documentation with relative links only
- ZIL source files (archived)
- Old kernel experiments (archived)
- Session summaries

### Phase 2: Script Reorganization (Script Updates Only)
Move scripts and update script-to-script calls:
- Update 10-15 script files
- Test each script after moving

### Phase 3: Kernel Reorganization (C++ Updates)
Move kernels and update host programs:
- Move kernels to `active/` and `archive/`
- Update 3 critical host programs
- Update 6 archived host programs
- Rebuild and test

### Phase 4: Documentation (Low Risk)
Update documentation links:
- Can be done gradually
- Broken links are annoying but not critical

---

## Automated Fix Script

I can create a script to automatically update these paths:

```bash
#!/bin/bash
# scripts/update-paths-after-reorg.sh

# Update host programs
sed -i 's|kernels/zork_interpreter_opt.cpp|kernels/active/zork_interpreter_opt.cpp|g' \
  host_programs/zork_on_blackhole.cpp \
  host_programs/archive/test_zork_optimized.cpp

sed -i 's|kernels/hello_riscv.cpp|kernels/active/hello_riscv.cpp|g' \
  host_programs/archive/test_hello_kernel.cpp

# Update script references
sed -i 's|./scripts/build_riscv.sh|./scripts/build/build_riscv.sh|g' \
  scripts/build/deploy.sh

# ... more automated updates
```

---

## Recommended Approach

### Option A: Minimal Reorganization
Only move documentation and archives, keep active files in place:
- ✅ Zero code changes needed
- ✅ Immediate improvement (docs organized)
- ⚠️ Scripts still in root `scripts/` dir

### Option B: Full Reorganization with Automation
Run full reorganization + automated path updates:
- ✅ Complete cleanup
- ✅ Automated fixes
- ⚠️ Need to test thoroughly after

### Option C: Phased Reorganization (Recommended)
Do it in phases over multiple commits:
1. **Commit 1**: Move docs and archives (safe)
2. **Commit 2**: Reorganize scripts + update references
3. **Commit 3**: Reorganize kernels + update host programs
4. **Commit 4**: Fix remaining doc links

**Benefits**:
- Each commit is small and testable
- Easy to rollback if issues
- Git history shows logical progression

---

## Testing After Reorganization

After each phase, test:

```bash
# Test builds still work
./scripts/build/build_local.sh
./scripts/build/build_riscv.sh

# Test LLM scripts
./scripts/llm/start-four-llms.sh
./scripts/llm/check-server-health.sh
./scripts/llm/stop-four-llms.sh

# Test hardware build (if applicable)
cd build-host && cmake --build . --parallel

# Test gameplay
./scripts/gameplay/play-zork-interactive.sh
```

---

## Decision Point

**Question for you**: Which approach do you prefer?

1. **Option A**: Minimal (docs only, no code changes)
2. **Option B**: Full automation (I'll create the fix script)
3. **Option C**: Phased (multiple commits, thoroughly tested)

I recommend **Option C** for safety, but I can implement whichever you prefer!

---

## Summary

**Total files needing updates**: ~25-30 files
- **Critical (blocks functionality)**: 3 host programs
- **Important (script calls)**: 10-15 scripts
- **Nice-to-have (docs)**: 10+ documentation files

**Work Required**:
- Low if Option A (docs only)
- Medium if Option C (phased)
- Medium-High if Option B (full automation + testing)

**Benefit**:
- Clean, professional repository structure
- Easy navigation for new contributors
- Clear separation of active vs historic files
