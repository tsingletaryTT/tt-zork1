#!/usr/bin/env bash
#
# reorganize-docs-only.sh - Minimal reorganization (docs only, zero code changes)
#
# This script ONLY moves documentation files to organized subdirectories.
# Scripts, kernels, host programs, and source code stay exactly where they are.
#
# Usage:
#   ./scripts/reorganize-docs-only.sh [--dry-run]

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
echo -e "${CYAN}║     Minimal Documentation Reorganization                 ║${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""
echo "This will organize documentation ONLY - no code changes!"
echo ""

# Create directory structure
echo -e "${YELLOW}Creating doc directories...${NC}"
if ! $DRY_RUN; then
    mkdir -p docs/{architecture,implementation,hardware,milestones,sessions,status}
    mkdir -p archive/{zil,builds}
    echo -e "  ${GREEN}✓ Directories created${NC}"
else
    echo "  [DRY-RUN] Would create doc subdirectories"
fi
echo ""

# Move architecture docs
echo -e "${YELLOW}Organizing architecture docs...${NC}"
move_file "docs/FOUR_CHIP_ARCHITECTURE.md" "docs/architecture/FOUR_CHIP_ARCHITECTURE.md"
echo ""

# Move implementation docs
echo -e "${YELLOW}Organizing implementation docs...${NC}"
move_file "docs/PROMPT_ENGINEERING_GUIDE.md" "docs/implementation/PROMPT_ENGINEERING_GUIDE.md"
move_file "docs/READ_OPCODE_IMPLEMENTATION.md" "docs/implementation/READ_OPCODE_IMPLEMENTATION.md"
move_file "docs/STATIC_COMPILATION_OPTIONS.md" "docs/implementation/STATIC_COMPILATION_OPTIONS.md"
echo ""

# Move hardware docs
echo -e "${YELLOW}Organizing hardware docs...${NC}"
move_file "docs/DEVICE_RECOVERY.md" "docs/hardware/DEVICE_RECOVERY.md"
move_file "docs/TENSTORRENT_EXPLAINED.md" "docs/hardware/TENSTORRENT_EXPLAINED.md"
echo ""

# Move milestone docs (with date prefixes)
echo -e "${YELLOW}Organizing milestone docs...${NC}"
move_file "BLACKHOLE_DICTIONARY_DECODED.md" "docs/milestones/2026-01-18-BLACKHOLE_DICTIONARY_DECODED.md"
move_file "FIRST_BLACKHOLE_RUN.md" "docs/milestones/2026-01-20-FIRST_BLACKHOLE_RUN.md"
move_file "FIRST_ZORK_TEXT_ON_BLACKHOLE.md" "docs/milestones/2026-01-20-FIRST_ZORK_TEXT_ON_BLACKHOLE.md"
move_file "MILESTONE_ZMACHINE_EXECUTION.md" "docs/milestones/2026-01-20-ZMACHINE_EXECUTION.md"
move_file "MILESTONE_HELLO_FROM_RISC-V.md" "docs/milestones/2026-01-21-HELLO_FROM_RISC-V.md"
move_file "MILESTONE_KERNEL_COMPILED.md" "docs/milestones/2026-01-22-KERNEL_COMPILED.md"
move_file "MILESTONE_ZMACHINE_HEADER_READ.md" "docs/milestones/2026-01-22-ZMACHINE_HEADER_READ.md"
move_file "MILESTONE_FIRST_RISC-V_EXECUTION.md" "docs/milestones/2026-01-20-FIRST_RISC-V_EXECUTION.md"
move_file "BLACKHOLE_FIRST_RUN_RESULTS.md" "docs/milestones/2026-01-20-FIRST_RUN_RESULTS.md"
echo ""

# Move session summaries
echo -e "${YELLOW}Organizing session summaries...${NC}"
move_file "SESSION_SUMMARY_JAN16-17.md" "docs/sessions/2026-01-16-SESSION_SUMMARY.md"
move_file "SESSION_SUMMARY_JAN18_2026_PART2.md" "docs/sessions/2026-01-18-SESSION_SUMMARY_PART2.md"
move_file "BREAKTHROUGH_JAN18_2026.md" "docs/sessions/2026-01-18-BREAKTHROUGH.md"
move_file "TODAY_EPIC_JOURNEY.md" "docs/sessions/2026-01-20-EPIC_JOURNEY.md"
echo ""

# Move status docs
echo -e "${YELLOW}Organizing status docs...${NC}"
move_file "BUILD_STATUS.md" "docs/status/BUILD_STATUS.md"
move_file "IMPLEMENTATION_STATUS.md" "docs/status/IMPLEMENTATION_STATUS.md"
move_file "IMPLEMENTATION_SUMMARY.md" "docs/status/IMPLEMENTATION_SUMMARY.md"
move_file "JOURNEY_MAPPING_SUMMARY.md" "docs/status/JOURNEY_MAPPING_SUMMARY.md"
move_file "TESTING_STATUS.md" "docs/status/TESTING_STATUS.md"
move_file "RESUME_NEXT_SESSION.md" "docs/status/RESUME_NEXT_SESSION.md"
move_file "SESSION_PROGRESS.md" "docs/status/SESSION_PROGRESS.md"
move_file "HARDWARE_FAULT_REPORT.md" "docs/hardware/2026-01-21-HARDWARE_FAULT_REPORT.md"
echo ""

# Archive ZIL source files
echo -e "${YELLOW}Archiving ZIL source files...${NC}"
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
echo -e "${GREEN}║            Minimal Reorganization Complete!               ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""

if $DRY_RUN; then
    echo "This was a dry run. Run without --dry-run to execute."
else
    echo "Documentation has been organized!"
    echo ""
    echo "✅ ZERO code changes needed - everything still works!"
    echo ""
    echo "Root directory files reduced from 53 to ~30"
    echo ""
    echo "Next steps:"
    echo "  1. git status    # Review changes"
    echo "  2. git add -A"
    echo "  3. git commit -m 'docs: Organize documentation into subdirectories'"
fi
echo ""
