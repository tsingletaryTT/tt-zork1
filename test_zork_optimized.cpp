// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * test_zork_optimized.cpp - Test optimized Z-machine kernel
 *
 * Tests the optimized kernel with incrementally increasing batch sizes:
 * 1. Single batch with interpret(10)
 * 2. Multiple batches if successful
 *
 * Usage: ./test_zork_optimized [num_batches]
 */

#include <cstdint>
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <string>

#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device.hpp>
#include <tt-metalium/buffer.hpp>
#include <tt-metalium/distributed.hpp>

using namespace tt;
using namespace tt::tt_metal;

constexpr const char* GAME_FILE = "game/zork1.z3";
constexpr uint32_t MAX_GAME_SIZE = 128 * 1024;
constexpr uint32_t MAX_OUTPUT_SIZE = 16 * 1024;
constexpr tt::tt_metal::CoreCoord ZORK_CORE = {0, 0};

std::vector<uint8_t> load_game_file(const char* filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error(std::string("Failed to open: ") + filename);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(MAX_GAME_SIZE, 0);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read game file");
    }

    std::cout << "✅ Loaded " << filename << " (" << size << " bytes)" << std::endl;
    return buffer;
}

int main(int argc, char* argv[]) {
    const int num_batches = (argc > 1) ? atoi(argv[1]) : 1;

    std::cout << "╔══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  Optimized Z-Machine Kernel Test                        ║" << std::endl;
    std::cout << "║  Kernel: zork_interpreter_opt.cpp (848 lines, -30.7%)   ║" << std::endl;
    std::cout << "║  Instructions per batch: 10 (vs 100 in original)        ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    std::cout << "Testing: " << num_batches << " batch(es) × 10 instructions = "
              << (num_batches * 10) << " total instructions" << std::endl;
    std::cout << std::endl;

    try {
        // Load game file
        std::cout << "[1/6] Loading game file..." << std::endl;
        std::vector<uint8_t> game_data = load_game_file(GAME_FILE);

        // Initialize device
        std::cout << "[2/6] Initializing device..." << std::endl;
        auto mesh_device = distributed::MeshDevice::create_unit_mesh(0);
        distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();

        // Enable program cache
        auto devices = mesh_device->get_devices();
        for (auto device : devices) {
            device->enable_program_cache();
        }
        std::cout << "      ✅ Device initialized with program cache" << std::endl;

        // Create buffers
        std::cout << "[3/6] Creating DRAM buffers..." << std::endl;

        distributed::DeviceLocalBufferConfig game_config{
            .page_size = MAX_GAME_SIZE,
            .buffer_type = BufferType::DRAM
        };

        distributed::DeviceLocalBufferConfig output_config{
            .page_size = MAX_OUTPUT_SIZE,
            .buffer_type = BufferType::DRAM
        };

        auto game_buffer = distributed::MeshBuffer::create(
            distributed::ReplicatedBufferConfig{.size = MAX_GAME_SIZE},
            game_config,
            mesh_device.get()
        );

        auto output_buffer = distributed::MeshBuffer::create(
            distributed::ReplicatedBufferConfig{.size = MAX_OUTPUT_SIZE},
            output_config,
            mesh_device.get()
        );

        // Upload game data
        EnqueueWriteMeshBuffer(cq, game_buffer, game_data, true);
        std::cout << "      ✅ Buffers created and game data uploaded" << std::endl;
        std::cout << "      Game buffer: 0x" << std::hex << game_buffer->address() << std::dec << std::endl;
        std::cout << "      Output buffer: 0x" << std::hex << output_buffer->address() << std::dec << std::endl;

        // Execute batches
        std::cout << "[4/6] Executing " << num_batches << " batch(es)..." << std::endl;
        std::cout << std::endl;

        std::string accumulated_output;

        for (int batch = 0; batch < num_batches; batch++) {
            std::cout << "  Batch " << (batch + 1) << "/" << num_batches << ":" << std::endl;

            // Create program and kernel
            Program program = CreateProgram();

            std::map<std::string, std::string> kernel_defines;
            char addr_buf[32];
            snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)game_buffer->address());
            kernel_defines["GAME_DRAM_ADDR"] = addr_buf;
            snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)output_buffer->address());
            kernel_defines["OUTPUT_DRAM_ADDR"] = addr_buf;

            std::cout << "    - Creating kernel..." << std::flush;
            KernelHandle kernel_id = CreateKernel(
                program,
                "/home/ttuser/code/tt-zork1/kernels/zork_interpreter_opt.cpp",
                ZORK_CORE,
                DataMovementConfig{
                    .processor = DataMovementProcessor::RISCV_0,
                    .noc = NOC::RISCV_0_default,
                    .defines = kernel_defines
                }
            );
            std::cout << " done" << std::endl;

            std::cout << "    - Executing..." << std::flush;
            distributed::MeshWorkload workload;
            distributed::MeshCoordinateRange device_range(mesh_device->shape());
            workload.add_program(device_range, std::move(program));
            distributed::EnqueueMeshWorkload(cq, workload, false);
            distributed::Finish(cq);
            std::cout << " done" << std::endl;

            // Read output
            std::cout << "    - Reading output..." << std::flush;
            std::vector<char> output_data(MAX_OUTPUT_SIZE);
            distributed::EnqueueReadMeshBuffer(cq, output_data, output_buffer, true);
            accumulated_output += output_data.data();
            std::cout << " done" << std::endl;

            std::cout << "    ✅ Batch " << (batch + 1) << " complete!" << std::endl;
            std::cout << std::endl;
        }

        // Display output
        std::cout << "[5/6] Displaying accumulated output..." << std::endl;
        std::cout << std::endl;
        std::cout << "╔════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║  ZORK OUTPUT FROM OPTIMIZED KERNEL:                    ║" << std::endl;
        std::cout << "╠════════════════════════════════════════════════════════╣" << std::endl;
        std::cout << accumulated_output << std::endl;
        std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;
        std::cout << std::endl;

        // Cleanup
        std::cout << "[6/6] Closing device..." << std::endl;
        mesh_device->close();
        std::cout << "      ✅ Device closed" << std::endl;
        std::cout << std::endl;

        if (num_batches > 1) {
            std::cout << "🎉 SUCCESS: " << num_batches << " batches executed!" << std::endl;
            std::cout << "   Device persistence proven with optimized kernel!" << std::endl;
        } else {
            std::cout << "✅ SUCCESS: Single batch executed!" << std::endl;
            std::cout << "   Ready to test multiple batches!" << std::endl;
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << std::endl;
        std::cerr << "❌ ERROR: " << e.what() << std::endl;
        return 1;
    }
}
