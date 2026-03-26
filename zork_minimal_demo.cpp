// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_minimal_demo.cpp - Host program for minimal Zork demo on Blackhole RISC-V
 *
 * This program demonstrates loading Zork I game data on Tenstorrent Blackhole
 * RISC-V cores and extracting the opening location text without running a full
 * Z-machine interpreter.
 *
 * What it does:
 * 1. Loads Zork I game file (zork1.z3) into DRAM
 * 2. Runs minimal kernel on RISC-V core that:
 *    - Copies game header to L1 memory
 *    - Reads Z-machine header fields
 *    - Decodes object name "West of House" using Z-string decoder
 *    - Generates demo output showing successful data loading
 * 3. Reads output from DRAM and displays it
 *
 * This proves the foundational data pipeline:
 *   DRAM → L1 → RISC-V processing → L1 → DRAM → Host display
 *
 * Usage:
 *   ./zork_minimal_demo [game_file]
 *   Default game_file: ../game/zork1.z3
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>

#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device.hpp>
#include <tt-metalium/distributed.hpp>

using namespace tt;
using namespace tt::tt_metal;

// Constants
constexpr uint32_t MAX_GAME_SIZE = 128 * 1024;   // 128KB for game file
constexpr uint32_t MAX_OUTPUT_SIZE = 16 * 1024;  // 16KB for output
constexpr CoreCoord TEST_CORE = CoreCoord(0, 0); // RISC-V core to use

int main(int argc, char* argv[]) {
    std::cout << "\n╔════════════════════════════════════════════════════╗\n";
    std::cout << "║  ZORK MINIMAL DEMO - BLACKHOLE RISC-V             ║\n";
    std::cout << "╚════════════════════════════════════════════════════╝\n\n";

    // Parse arguments
    std::string game_file = (argc > 1) ? argv[1] : "../game/zork1.z3";

    std::cout << "[1/8] Loading game file: " << game_file << "\n";

    // Load game file
    std::ifstream file(game_file, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "ERROR: Failed to open game file: " << game_file << "\n";
        return 1;
    }

    std::streamsize game_size = file.tellg();
    if (game_size > MAX_GAME_SIZE) {
        std::cerr << "ERROR: Game file too large: " << game_size << " > " << MAX_GAME_SIZE << "\n";
        return 1;
    }

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> game_data(MAX_GAME_SIZE, 0);
    file.read(reinterpret_cast<char*>(game_data.data()), game_size);
    file.close();

    std::cout << "  ✓ Loaded " << game_size << " bytes\n";

    // Initialize device
    std::cout << "[2/8] Initializing Blackhole device...\n";

    auto mesh_device = distributed::MeshDevice::create_unit_mesh(0);
    if (!mesh_device) {
        std::cerr << "ERROR: Failed to create mesh device\n";
        return 1;
    }

    std::cout << "  ✓ Device initialized\n";

    // Create DRAM buffers with contiguous pages
    std::cout << "[3/8] Allocating DRAM buffers...\n";

    distributed::DeviceLocalBufferConfig game_dram_config{
        .page_size = MAX_GAME_SIZE,
        .buffer_type = BufferType::DRAM
    };

    distributed::DeviceLocalBufferConfig output_dram_config{
        .page_size = MAX_OUTPUT_SIZE,
        .buffer_type = BufferType::DRAM
    };

    auto game_buffer = distributed::MeshBuffer::create(
        distributed::ReplicatedBufferConfig{.size = MAX_GAME_SIZE},
        game_dram_config,
        mesh_device.get()
    );

    auto output_buffer = distributed::MeshBuffer::create(
        distributed::ReplicatedBufferConfig{.size = MAX_OUTPUT_SIZE},
        output_dram_config,
        mesh_device.get()
    );

    std::cout << "  ✓ Game buffer: " << MAX_GAME_SIZE << " bytes\n";
    std::cout << "  ✓ Output buffer: " << MAX_OUTPUT_SIZE << " bytes\n";

    // Upload game data to DRAM
    std::cout << "[4/8] Uploading game data to DRAM...\n";

    distributed::EnqueueWriteMeshBuffer(mesh_device->mesh_command_queue(), game_buffer, game_data, false);
    distributed::Finish(mesh_device->mesh_command_queue());

    std::cout << "  ✓ Game data uploaded\n";

    // Create program and kernel
    std::cout << "[5/8] Creating kernel...\n";

    auto program = CreateProgram();

    // Get DRAM addresses
    uint64_t game_dram_addr = game_buffer->address();
    uint64_t output_dram_addr = output_buffer->address();

    // Create kernel with compile-time defines for DRAM addresses
    std::map<std::string, std::string> defines;
    defines["GAME_DRAM_ADDR"] = std::to_string(game_dram_addr);
    defines["OUTPUT_DRAM_ADDR"] = std::to_string(output_dram_addr);

    KernelHandle kernel_id = CreateKernel(
        program,
        "/home/ttuser/code/tt-zork1/kernels/zork_ultra_minimal.cpp",
        TEST_CORE,
        DataMovementConfig{
            .processor = DataMovementProcessor::RISCV_0,
            .noc = NOC::RISCV_0_default,
            .defines = defines
        }
    );

    std::cout << "  ✓ Kernel created\n";

    // Execute kernel
    std::cout << "[6/8] Executing kernel on RISC-V...\n";

    distributed::MeshWorkload workload;
    distributed::MeshCoordinateRange device_range(mesh_device->shape());
    workload.add_program(device_range, std::move(program));

    distributed::EnqueueMeshWorkload(mesh_device->mesh_command_queue(), workload, false);
    distributed::Finish(mesh_device->mesh_command_queue());

    std::cout << "  ✓ Kernel execution complete\n";

    // Read output
    std::cout << "[7/8] Reading output from DRAM...\n";

    std::vector<uint8_t> output_data(MAX_OUTPUT_SIZE);
    distributed::EnqueueReadMeshBuffer(mesh_device->mesh_command_queue(), output_data, output_buffer, true);

    std::cout << "  ✓ Output retrieved\n";

    // Display output
    std::cout << "[8/8] Displaying result:\n\n";
    std::cout << "╔════════════════════════════════════════════════════╗\n";
    std::cout << "║  ZORK OUTPUT FROM BLACKHOLE RISC-V CORE           ║\n";
    std::cout << "╠════════════════════════════════════════════════════╣\n";

    // Find null terminator
    size_t output_len = 0;
    for (size_t i = 0; i < output_data.size(); i++) {
        if (output_data[i] == 0) {
            output_len = i;
            break;
        }
    }

    if (output_len > 0) {
        std::cout << std::string(reinterpret_cast<char*>(output_data.data()), output_len);
    } else {
        std::cout << "(no output)\n";
    }

    std::cout << "╚════════════════════════════════════════════════════╝\n";

    // Cleanup
    mesh_device->close();

    std::cout << "\n✅ Demo complete!\n\n";

    return 0;
}
