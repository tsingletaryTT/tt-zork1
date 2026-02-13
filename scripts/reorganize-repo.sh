#!/usr/bin/env bash
#
# reorganize-repo.sh - Execute directory reorganization plan
#
# This script implements the cleanup plan from docs/DIRECTORY_CLEANUP_PLAN.md
# It preserves all historic files while improving organization.
#
# Usage:
#   ./scripts/reorganize-repo.sh [--dry-run]
#
# Options:
#   --dry-run    Show what would be done without making changes

set -e

DRY_RUN=false
if [ "$1" = "--dry-run" ]; then
    DRY_RUN=true
    echo "=== DRY RUN MODE - No changes will be made ==="
    echo ""
fi

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Move command (dry-run aware)
move_file() {
    local src="$1"
    local dst="$2"

    if [ ! -e "$src" ]; then
        return 0  # Skip if source doesn't exist
    fi

    if $DRY_RUN; then
        echo "  [DRY-RUN] mv $src $dst"
    else
        mkdir -p "$(dirname "$dst")"
        git mv "$src" "$dst" 2>/dev/null || mv "$src" "$dst"
        echo -e "  ${GREEN}✓${NC} $src → $dst"
    fi
}

echo -e "${CYAN}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║         Repository Reorganization Script                 ║${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""

# Phase 1: Create directories
echo -e "${YELLOW}Phase 1: Creating directory structure...${NC}"
if ! $DRY_RUN; then
    mkdir -p docs/{architecture,implementation,hardware,milestones,sessions,status}
    mkdir -p kernels/{active,archive/{exploration,decoding,interpreters}}
    mkdir -p host_programs/archive
    mkdir -p tests/integration
    mkdir -p scripts/{build,llm,testing,gameplay,utils}
    mkdir -p archive/{zil,builds}
    echo -e "  ${GREEN}✓ Directories created${NC}"
else
    echo "  [DRY-RUN] Would create organized directory structure"
fi
echo ""

# Phase 2: Move documentation
echo -e "${YELLOW}Phase 2: Organizing documentation...${NC}"

# Architecture docs
move_file "docs/FOUR_CHIP_ARCHITECTURE.md" "docs/architecture/FOUR_CHIP_ARCHITECTURE.md"

# Implementation docs
move_file "docs/PROMPT_ENGINEERING_GUIDE.md" "docs/implementation/PROMPT_ENGINEERING_GUIDE.md"
move_file "docs/READ_OPCODE_IMPLEMENTATION.md" "docs/implementation/READ_OPCODE_IMPLEMENTATION.md"

# Hardware docs
move_file "docs/DEVICE_RECOVERY.md" "docs/hardware/DEVICE_RECOVERY.md"
move_file "docs/TENSTORRENT_EXPLAINED.md" "docs/hardware/TENSTORRENT_EXPLAINED.md"

# Milestones (with date prefixes)
move_file "BLACKHOLE_DICTIONARY_DECODED.md" "docs/milestones/2026-01-18-BLACKHOLE_DICTIONARY_DECODED.md"
move_file "FIRST_BLACKHOLE_RUN.md" "docs/milestones/2026-01-20-FIRST_BLACKHOLE_RUN.md"
move_file "FIRST_ZORK_TEXT_ON_BLACKHOLE.md" "docs/milestones/2026-01-20-FIRST_ZORK_TEXT_ON_BLACKHOLE.md"
move_file "MILESTONE_ZMACHINE_EXECUTION.md" "docs/milestones/2026-01-20-ZMACHINE_EXECUTION.md"
move_file "MILESTONE_HELLO_FROM_RISC-V.md" "docs/milestones/2026-01-21-HELLO_FROM_RISC-V.md"
move_file "MILESTONE_KERNEL_COMPILED.md" "docs/milestones/2026-01-22-KERNEL_COMPILED.md"
move_file "MILESTONE_ZMACHINE_HEADER_READ.md" "docs/milestones/2026-01-22-ZMACHINE_HEADER_READ.md"
move_file "MILESTONE_FIRST_RISC-V_EXECUTION.md" "docs/milestones/2026-01-20-FIRST_RISC-V_EXECUTION.md"

# Session summaries
move_file "SESSION_SUMMARY_JAN16-17.md" "docs/sessions/2026-01-16-SESSION_SUMMARY.md"
move_file "SESSION_SUMMARY_JAN18_2026_PART2.md" "docs/sessions/2026-01-18-SESSION_SUMMARY_PART2.md"
move_file "BREAKTHROUGH_JAN18_2026.md" "docs/sessions/2026-01-18-BREAKTHROUGH.md"
move_file "TODAY_EPIC_JOURNEY.md" "docs/sessions/2026-01-20-EPIC_JOURNEY.md"

# Status docs
move_file "BUILD_STATUS.md" "docs/status/BUILD_STATUS.md"
move_file "IMPLEMENTATION_STATUS.md" "docs/status/IMPLEMENTATION_STATUS.md"
move_file "IMPLEMENTATION_SUMMARY.md" "docs/status/IMPLEMENTATION_SUMMARY.md"
move_file "JOURNEY_MAPPING_SUMMARY.md" "docs/status/JOURNEY_MAPPING_SUMMARY.md"
move_file "TESTING_STATUS.md" "docs/status/TESTING_STATUS.md"
move_file "RESUME_NEXT_SESSION.md" "docs/status/RESUME_NEXT_SESSION.md"
move_file "SESSION_PROGRESS.md" "docs/status/SESSION_PROGRESS.md"
move_file "BLACKHOLE_FIRST_RUN_RESULTS.md" "docs/milestones/2026-01-20-FIRST_RUN_RESULTS.md"
move_file "HARDWARE_FAULT_REPORT.md" "docs/hardware/2026-01-21-HARDWARE_FAULT_REPORT.md"

echo ""

# Phase 3: Organize kernels
echo -e "${YELLOW}Phase 3: Organizing kernels...${NC}"

# Active kernels
move_file "kernels/zork_interpreter_opt.cpp" "kernels/active/zork_interpreter_opt.cpp"
move_file "kernels/hello_riscv.cpp" "kernels/active/hello_riscv.cpp"

# Archive - exploration
for f in kernels/test_*.cpp kernels/zork_scan_*.cpp kernels/zork_find_*.cpp kernels/zork_opening_*.cpp; do
    [ -e "$f" ] && move_file "$f" "kernels/archive/exploration/$(basename "$f")"
done
move_file "kernels/zork_high_memory_scan.cpp" "kernels/archive/exploration/zork_high_memory_scan.cpp"
move_file "kernels/zork_verify_load.cpp" "kernels/archive/exploration/zork_verify_load.cpp"

# Archive - decoding
for f in kernels/zork_decode_*.cpp kernels/zork_object*.cpp kernels/zork_dict*.cpp; do
    [ -e "$f" ] && move_file "$f" "kernels/archive/decoding/$(basename "$f")"
done
move_file "kernels/zork_frotz_decode.cpp" "kernels/archive/decoding/zork_frotz_decode.cpp"

# Archive - interpreters
move_file "kernels/zork_interpreter_v1.cpp" "kernels/archive/interpreters/zork_interpreter_v1.cpp"
move_file "kernels/zork_interpreter.cpp" "kernels/archive/interpreters/zork_interpreter.cpp"
move_file "kernels/zork_monolithic.cpp" "kernels/archive/interpreters/zork_monolithic.cpp"
for f in kernels/zork_execute*.cpp kernels/zork_frotz_*.cpp; do
    [ -e "$f" ] && move_file "$f" "kernels/archive/interpreters/$(basename "$f")"
done
move_file "kernels/zork_kernel.cpp" "kernels/archive/interpreters/zork_kernel.cpp"
move_file "kernels/zork_minimal_executor.cpp" "kernels/archive/interpreters/zork_minimal_executor.cpp"

# Other kernels to archive
for f in kernels/noop_riscv.cpp kernels/simple_test.cpp; do
    [ -e "$f" ] && move_file "$f" "kernels/archive/exploration/$(basename "$f")"
done

echo ""

# Phase 4: Organize host programs
echo -e "${YELLOW}Phase 4: Organizing host programs...${NC}"

# Main programs
move_file "zork_on_blackhole.cpp" "host_programs/zork_on_blackhole.cpp"
move_file "zork_batched.cpp" "host_programs/zork_batched.cpp"

# Archive
move_file "zork_on_blackhole_streaming.cpp" "host_programs/archive/zork_on_blackhole_streaming.cpp"
for f in test_*.cpp; do
    [ -e "$f" ] && move_file "$f" "host_programs/archive/$(basename "$f")"
done

echo ""

# Phase 5: Organize tests
echo -e "${YELLOW}Phase 5: Organizing tests...${NC}"

move_file "test_router.c" "tests/integration/test_router.c"
move_file "test-map-visual" "tests/integration/test-map-visual"
move_file "test-map-visual.c" "tests/integration/test-map-visual.c"

echo ""

# Phase 6: Organize scripts
echo -e "${YELLOW}Phase 6: Organizing scripts...${NC}"

# Build scripts
move_file "scripts/build_local.sh" "scripts/build/build_local.sh"
move_file "scripts/build_riscv.sh" "scripts/build/build_riscv.sh"
move_file "scripts/build_blackhole_kernel.sh" "scripts/build/build_blackhole_kernel.sh"
move_file "scripts/deploy.sh" "scripts/build/deploy.sh"

# LLM scripts
for f in scripts/*llm*.sh scripts/check-server-health.sh scripts/get-model-names.sh; do
    [ -e "$f" ] && move_file "$f" "scripts/llm/$(basename "$f")"
done

# Testing scripts
for f in scripts/test-*.sh scripts/compare-prompts.sh scripts/quick-test-translator.sh scripts/tune-prompt-interactive.sh; do
    [ -e "$f" ] && move_file "$f" "scripts/testing/$(basename "$f")"
done

# Gameplay scripts
for f in play-*.sh run-zork-*.sh demo-*.sh test-map-demo.sh; do
    [ -e "$f" ] && move_file "$f" "scripts/gameplay/$(basename "$f")"
done

# Utilities
move_file "scripts/tt-cold-reboot.sh" "scripts/utils/tt-cold-reboot.sh"
move_file "scripts/extract-command.sh" "scripts/utils/extract-command.sh"

echo ""

# Phase 7: Archive ZIL source
echo -e "${YELLOW}Phase 7: Archiving ZIL source files...${NC}"

for f in *.zil; do
    [ -e "$f" ] && move_file "$f" "archive/zil/$(basename "$f")"
done
for f in zork1.*; do
    [ -e "$f" ] && move_file "$f" "archive/zil/$(basename "$f")"
done
move_file "parser.cmp" "archive/zil/parser.cmp"
move_file "COMPILED" "archive/builds/COMPILED"

echo ""

# Summary
echo -e "${GREEN}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║                Reorganization Complete!                   ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""

if $DRY_RUN; then
    echo "This was a dry run. No changes were made."
    echo "Run without --dry-run to execute the reorganization."
else
    echo "Directory structure has been reorganized."
    echo ""
    echo "Next steps:"
    echo "  1. Review changes: git status"
    echo "  2. Test builds still work"
    echo "  3. Commit changes:"
    echo "     git add -A"
    echo "     git commit -m 'refactor: Reorganize directory structure for clarity'"
fi
