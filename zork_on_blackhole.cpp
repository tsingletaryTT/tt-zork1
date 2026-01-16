// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * zork_on_blackhole.cpp - Run Zork I on Tenstorrent Blackhole RISC-V cores
 *
 * This program demonstrates running a classic text adventure game (Zork, 1977)
 * on AI accelerator hardware by wrapping the Z-machine interpreter as a
 * TT-Metal kernel executing on RISC-V data movement cores.
 *
 * Architecture:
 * - Host (x86): Loads game file, manages I/O buffers, displays output
 * - Device (RISC-V): Runs Z-machine interpreter, processes game logic
 * - Communication: Via DRAM buffers for game data, input, and output
 *
 * Historic Note: This is likely the first text adventure game ever run on
 * AI accelerator hardware, bridging 1977 gaming with 2026 AI technology.
 */

#include <cstdint>
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <string>
#include <cstring>

#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device.hpp>
#include "tt-metalium/buffer.hpp"
#include <tt-metalium/distributed.hpp>

using namespace tt;
using namespace tt::tt_metal;

// Game file path
constexpr const char* DEFAULT_GAME_FILE = "game/zork1.z3";

// Buffer sizes
constexpr uint32_t MAX_GAME_SIZE = 128 * 1024;  // 128KB for game file
constexpr uint32_t MAX_INPUT_SIZE = 1024;        // 1KB for user input
constexpr uint32_t MAX_OUTPUT_SIZE = 16 * 1024;  // 16KB for game output

// Core to run kernel on
constexpr CoreCoord ZORK_CORE = {0, 0};

/**
 * Load game file from filesystem into memory buffer
 */
std::vector<uint8_t> load_game_file(const char* filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error(std::string("Failed to open game file: ") + filename);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size > MAX_GAME_SIZE) {
        throw std::runtime_error("Game file too large for buffer");
    }

    std::vector<uint8_t> buffer(MAX_GAME_SIZE, 0);  // Pad to full size
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read game file");
    }

    std::cout << "Loaded game file: " << filename << " (" << size << " bytes)" << std::endl;
    return buffer;
}

/**
 * Main entry point
 */
int main(int argc, char* argv[]) {
    const char* game_file = (argc > 1) ? argv[1] : DEFAULT_GAME_FILE;

    std::cout << "╔══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  ZORK I on Tenstorrent Blackhole RISC-V Cores" << std::endl;
    std::cout << "║  1977 Game on 2026 AI Accelerator" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    try {
        // Load game file from host filesystem
        std::cout << "[Host] Loading game file..." << std::endl;
        std::vector<uint8_t> game_data = load_game_file(game_file);

        // Initialize Tenstorrent device
        std::cout << "[Host] Initializing Blackhole device..." << std::endl;
        std::shared_ptr<distributed::MeshDevice> mesh_device =
            distributed::MeshDevice::create_unit_mesh(0);

        distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();
        distributed::MeshWorkload workload;
        distributed::MeshCoordinateRange device_range =
            distributed::MeshCoordinateRange(mesh_device->shape());

        // Configure DRAM buffers for game data, input, and output
        std::cout << "[Host] Allocating DRAM buffers..." << std::endl;

        distributed::DeviceLocalBufferConfig dram_config{
            .page_size = 1024,  // 1KB pages
            .buffer_type = BufferType::DRAM
        };

        // Create buffers
        auto game_buffer = distributed::MeshBuffer::create(
            distributed::ReplicatedBufferConfig{.size = MAX_GAME_SIZE},
            dram_config,
            mesh_device.get()
        );

        auto input_buffer = distributed::MeshBuffer::create(
            distributed::ReplicatedBufferConfig{.size = MAX_INPUT_SIZE},
            dram_config,
            mesh_device.get()
        );

        auto output_buffer = distributed::MeshBuffer::create(
            distributed::ReplicatedBufferConfig{.size = MAX_OUTPUT_SIZE},
            dram_config,
            mesh_device.get()
        );

        // Upload game data to DRAM
        std::cout << "[Host] Uploading game data to device DRAM..." << std::endl;
        EnqueueWriteMeshBuffer(cq, game_buffer, game_data, /*blocking=*/true);

        std::cout << "[Host] Device initialized successfully!" << std::endl;
        std::cout << "       - Game data: " << game_data.size() << " bytes in DRAM" << std::endl;
        std::cout << "       - Input buffer: " << MAX_INPUT_SIZE << " bytes" << std::endl;
        std::cout << "       - Output buffer: " << MAX_OUTPUT_SIZE << " bytes" << std::endl;
        std::cout << std::endl;

        // TODO: Create and load kernel
        std::cout << "[TODO] Next: Create Zork kernel and load onto RISC-V core {0,0}" << std::endl;
        std::cout << "[TODO] Next: Implement game loop with DRAM buffer I/O" << std::endl;

        // Cleanup
        mesh_device->close();

        std::cout << std::endl;
        std::cout << "✓ Proof of concept: Successfully loaded game data onto Blackhole!" << std::endl;
        std::cout << "  Next steps: Implement kernel and I/O adapters" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
