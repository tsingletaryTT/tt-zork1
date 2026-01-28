#!/bin/bash
# SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
# SPDX-License-Identifier: Apache-2.0

# Run Zork with persistent Python execution
# This script sets up the environment and runs zork_persistent.py

set -e

# Set library paths for TT-Metal
export LD_LIBRARY_PATH=/home/ttuser/tt-metal/build_Release/lib:$LD_LIBRARY_PATH
export TT_METAL_RUNTIME_ROOT=/home/ttuser/tt-metal

# Run Python script
python3 zork_persistent.py "$@"
