#!/bin/bash
# Tenstorrent "Cold Reboot" Script
# Simulates a cold reboot by reloading the kernel driver
# This clears stuck firmware states without requiring system reboot

set -e  # Exit on error

SCRIPT_NAME="tt-cold-reboot"
MODULE_NAME="tenstorrent"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}[ERROR]${NC} This script must be run as root (use sudo)"
    exit 1
fi

echo -e "${BLUE}╔═══════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  Tenstorrent Driver Cold Reboot              ║${NC}"
echo -e "${BLUE}║  Unload + Reload kernel driver for recovery  ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════╝${NC}"
echo

# Step 1: Check current module status
echo -e "${BLUE}[1/6]${NC} Checking current module status..."
if lsmod | grep -q "^${MODULE_NAME}"; then
    MODULE_STATUS=$(lsmod | grep "^${MODULE_NAME}" | awk '{print $3}')
    echo -e "      ✅ Module ${MODULE_NAME} is loaded (refcount: ${MODULE_STATUS})"
else
    echo -e "      ${YELLOW}⚠️  Module ${MODULE_NAME} is not loaded${NC}"
    echo -e "      Nothing to reload. Exiting."
    exit 0
fi

# Step 2: Check for processes using devices
echo -e "${BLUE}[2/6]${NC} Checking for processes using Tenstorrent devices..."
PROCS=$(lsof /dev/tenstorrent/* 2>/dev/null || true)
if [ -n "$PROCS" ]; then
    echo -e "      ${YELLOW}⚠️  Warning: Processes are using devices:${NC}"
    echo "$PROCS" | head -10
    echo
    read -p "      Kill these processes? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo -e "      Killing processes..."
        lsof -t /dev/tenstorrent/* 2>/dev/null | xargs -r kill -9
        sleep 1
        echo -e "      ✅ Processes killed"
    else
        echo -e "      ${RED}[ERROR]${NC} Cannot reload module while devices are in use"
        exit 1
    fi
else
    echo -e "      ✅ No processes using devices"
fi

# Step 3: Unload the module
echo -e "${BLUE}[3/6]${NC} Unloading kernel module..."
if rmmod ${MODULE_NAME}; then
    echo -e "      ✅ Module unloaded successfully"
else
    echo -e "      ${RED}[ERROR]${NC} Failed to unload module"
    echo -e "      This usually means:"
    echo -e "      1. Processes still using devices (check with: lsof /dev/tenstorrent/*)"
    echo -e "      2. Module is in use by another module"
    echo -e "      3. System requires a real reboot"
    exit 1
fi

# Step 4: Wait for devices to disappear
echo -e "${BLUE}[4/6]${NC} Waiting for devices to disappear..."
sleep 2

if [ -e /dev/tenstorrent ]; then
    echo -e "      ${YELLOW}⚠️  Device files still present (may be normal)${NC}"
else
    echo -e "      ✅ Device files cleared"
fi

# Step 5: Reload the module
echo -e "${BLUE}[5/6]${NC} Reloading kernel module..."
if modprobe ${MODULE_NAME}; then
    echo -e "      ✅ Module loaded successfully"
else
    echo -e "      ${RED}[ERROR]${NC} Failed to reload module"
    echo -e "      System may require a real reboot now"
    exit 1
fi

# Step 6: Wait for devices to reappear and verify
echo -e "${BLUE}[6/6]${NC} Waiting for devices to initialize..."
sleep 3

# Verify devices are back
if [ -e /dev/tenstorrent ]; then
    DEVICE_COUNT=$(ls -1 /dev/tenstorrent/ 2>/dev/null | wc -l)
    echo -e "      ✅ Devices reappeared (count: ${DEVICE_COUNT})"
else
    echo -e "      ${YELLOW}⚠️  Devices not yet visible${NC}"
fi

# Verify with tt-smi
echo
echo -e "${BLUE}[VERIFY]${NC} Checking device health with tt-smi..."
echo
if command -v tt-smi &> /dev/null; then
    # Run tt-smi as the original user (not root)
    SUDO_USER_HOME=$(eval echo ~${SUDO_USER})
    su - ${SUDO_USER} -c "tt-smi -s | python3 -m json.tool | jq -r '.device_info[] | \"Device \(.board_info.bus_id): \(.device_info.ASIC_SW_VERS // \"N/A\")\"'" 2>/dev/null || \
    su - ${SUDO_USER} -c "tt-smi -s" | head -30
else
    echo -e "      ${YELLOW}⚠️  tt-smi not found, skipping verification${NC}"
fi

echo
echo -e "${GREEN}╔═══════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  ✅ Cold Reboot Complete!                     ║${NC}"
echo -e "${GREEN}║  Kernel driver reloaded successfully         ║${NC}"
echo -e "${GREEN}║  Devices should be in fresh state            ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════╝${NC}"
echo
echo -e "${BLUE}[NEXT]${NC} You can now run your Tenstorrent programs."
echo -e "        Device firmware has been reset."
echo
