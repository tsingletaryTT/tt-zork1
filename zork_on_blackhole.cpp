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
#include <tt-metalium/system_mesh.hpp>

using namespace tt;
using namespace tt::tt_metal;

// Game file path
constexpr const char* DEFAULT_GAME_FILE = "game/zork1.z3";

// Buffer sizes
constexpr uint32_t MAX_GAME_SIZE = 128 * 1024;  // 128KB for game file
constexpr uint32_t MAX_INPUT_SIZE = 1024;        // 1KB for user input
constexpr uint32_t MAX_OUTPUT_SIZE = 16 * 1024;  // 16KB for game output
constexpr uint32_t MAX_STATE_SIZE = 16 * 1024;   // 16KB for Z-machine state

// Core to run kernel on
constexpr CoreCoord ZORK_CORE = {0, 0};

// State persistence file
constexpr const char* STATE_FILE = "/tmp/zork_state.bin";

// Input command file
constexpr const char* INPUT_FILE = "/tmp/zork_input.txt";

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
    const int num_batches = (argc > 2) ? atoi(argv[2]) : 1;  // Default: 1 batch (single-shot)

    std::cout << "╔══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  ZORK I on Tenstorrent Blackhole RISC-V Cores" << std::endl;
    std::cout << "║  1977 Game on 2026 AI Accelerator" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    if (num_batches > 1) {
        std::cout << "Batched execution: " << num_batches << " batches (each = 100 instructions)" << std::endl;
        std::cout << std::endl;
    }

    try {
        // Load game file from host filesystem
        std::cout << "[Host] Loading game file..." << std::endl;
        std::vector<uint8_t> game_data = load_game_file(game_file);

        // Initialize Tenstorrent device using parent_mesh → submesh pattern
        // This is the correct approach for multi-chip systems and may avoid
        // the core (x=1,y=2) initialization issue by using proper submesh APIs
        std::cout << "[Host] Initializing Blackhole device..." << std::endl;

        // Step 1: Get system mesh shape and create parent mesh for ALL devices
        const auto system_mesh_shape = distributed::SystemMesh::instance().shape();
        std::cout << "[Host] Creating parent mesh for system (shape: " << system_mesh_shape << ")..." << std::flush;

        std::shared_ptr<distributed::MeshDevice> parent_mesh =
            distributed::MeshDevice::create(distributed::MeshDeviceConfig(system_mesh_shape));
        std::cout << " done" << std::endl;

        // Step 2: Create 1x1 submesh for Zork execution
        // Use chip 1 (offset 1,0) to avoid chip 0's core (x=1,y=2) issue
        std::cout << "[Host] Creating 1x1 submesh for Zork execution (using chip 1)..." << std::flush;
        std::shared_ptr<distributed::MeshDevice> mesh_device =
            parent_mesh->create_submesh(
                distributed::MeshShape(1, 1),
                distributed::MeshCoordinate(1, 0)  // Start at chip 1 instead of chip 0
            );
        std::cout << " done" << std::endl;

        std::cout << "[Host] Device initialized successfully!" << std::endl;

        // ═══════════════════════════════════════════════════════
        // ENABLE PROGRAM CACHE - Critical for repeated execution!
        // Without this, second batch will fail with device errors
        // ═══════════════════════════════════════════════════════
        auto devices = mesh_device->get_devices();
        for (auto device : devices) {
            device->enable_program_cache();
        }
        std::cout << "[Host] Program cache enabled (allows multiple batches without reset)" << std::endl;

        distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();
        // NOTE: workload and device_range now declared later after buffer setup

        // Use DRAM for host-device communication (L1 might not be visible to host)
        std::cout << "[Host] Allocating DRAM buffers..." << std::endl;

        // For non-interleaved DRAM buffers, page_size must equal buffer size
        // Use separate configs for each buffer to ensure contiguous allocation
        distributed::DeviceLocalBufferConfig game_dram_config{
            .page_size = MAX_GAME_SIZE,  // 128KB page = whole buffer in one page
            .buffer_type = BufferType::DRAM
        };

        distributed::DeviceLocalBufferConfig output_dram_config{
            .page_size = MAX_OUTPUT_SIZE,  // 16KB page = whole buffer in one page
            .buffer_type = BufferType::DRAM
        };

        distributed::DeviceLocalBufferConfig input_dram_config{
            .page_size = MAX_INPUT_SIZE,  // 1KB page = whole buffer in one page
            .buffer_type = BufferType::DRAM
        };

        distributed::DeviceLocalBufferConfig state_dram_config{
            .page_size = MAX_STATE_SIZE,  // 16KB page = whole buffer in one page
            .buffer_type = BufferType::DRAM
        };

        // Create DRAM buffers for host-device communication
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

        auto input_buffer = distributed::MeshBuffer::create(
            distributed::ReplicatedBufferConfig{.size = MAX_INPUT_SIZE},
            input_dram_config,
            mesh_device.get()
        );

        auto state_buffer = distributed::MeshBuffer::create(
            distributed::ReplicatedBufferConfig{.size = MAX_STATE_SIZE},
            state_dram_config,
            mesh_device.get()
        );

        // Upload game data to DRAM
        std::cout << "[Host] Uploading game data to DRAM..." << std::endl;
        EnqueueWriteMeshBuffer(cq, game_buffer, game_data, /*blocking=*/true);

        // Load input from file if exists, otherwise use empty
        std::vector<uint8_t> input_data(MAX_INPUT_SIZE, 0);
        std::ifstream input_file(INPUT_FILE);
        if (input_file) {
            std::string input_line;
            std::getline(input_file, input_line);
            input_file.close();
            std::strncpy(reinterpret_cast<char*>(input_data.data()), input_line.c_str(), MAX_INPUT_SIZE - 1);
            std::cout << "[Host] Loaded input command: \"" << input_line << "\"" << std::endl;
        } else {
            std::cout << "[Host] No input file found, using empty input" << std::endl;
        }
        EnqueueWriteMeshBuffer(cq, input_buffer, input_data, /*blocking=*/true);

        // Load or initialize state buffer
        std::vector<uint8_t> state_data(MAX_STATE_SIZE, 0);
        std::ifstream state_file(STATE_FILE, std::ios::binary);
        if (state_file) {
            // Resume from previous run
            state_file.read(reinterpret_cast<char*>(state_data.data()), MAX_STATE_SIZE);
            state_file.close();
            std::cout << "[Host] Loaded previous state from " << STATE_FILE << std::endl;
        } else {
            // First run - start fresh
            std::cout << "[Host] No previous state found, starting fresh" << std::endl;
        }
        EnqueueWriteMeshBuffer(cq, state_buffer, state_data, /*blocking=*/true);

        std::cout << "[Host] Device initialized successfully!" << std::endl;
        std::cout << "       - Game data: " << game_data.size() << " bytes in DRAM" << std::endl;
        std::cout << "       - Game buffer at DRAM address: 0x" << std::hex << game_buffer->address() << std::dec << std::endl;
        std::cout << "       - Output buffer at DRAM address: 0x" << std::hex << output_buffer->address() << std::dec << std::endl;
        std::cout << "       - Input buffer at DRAM address: 0x" << std::hex << input_buffer->address() << std::dec << std::endl;
        std::cout << "       - State buffer at DRAM address: 0x" << std::hex << state_buffer->address() << std::dec << std::endl;
        std::cout << std::endl;

        // ═══════════════════════════════════════════════════════════════════════
        // CREATE PROGRAM AND WORKLOAD ONCE (Workload Reuse Pattern)
        // This is the TT-Metal best practice from zork_repl.cpp and test_mesh_workload.cpp
        // Previously we recreated Program + Kernel in every batch, causing hangs!
        // ═══════════════════════════════════════════════════════════════════════
        std::cout << "[Host] Creating program and workload (compile once, reuse many times)..." << std::flush;

        // Create program
        Program program = CreateProgram();

        // Pass buffer addresses as compile-time defines (done ONCE)
        std::map<std::string, std::string> kernel_defines;
        char addr_buf[32];
        snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)game_buffer->address());
        kernel_defines["GAME_DRAM_ADDR"] = addr_buf;
        snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)output_buffer->address());
        kernel_defines["OUTPUT_DRAM_ADDR"] = addr_buf;
        snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)input_buffer->address());
        kernel_defines["INPUT_DRAM_ADDR"] = addr_buf;
        // STATE PERSISTENCE ENABLED - Capturing hang with debug instrumentation
        // Baseline confirmed working (4 batches complete without state)
        // Now testing WITH state persistence to diagnose NoC hang
        snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)state_buffer->address());
        kernel_defines["STATE_DRAM_ADDR"] = addr_buf;

        // Create kernel (compiled once)
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

        // Create workload and add program (done ONCE!)
        distributed::MeshWorkload workload;
        distributed::MeshCoordinateRange device_range(mesh_device->shape());
        workload.add_program(device_range, std::move(program));

        std::cout << " done" << std::endl;
        std::cout << "[Host] Workload created successfully - ready to execute batches!" << std::endl;
        std::cout << std::endl;

        // ═══════════════════════════════════════════════════════════════════════
        // RUN BATCHES (Reusing the same workload object!)
        // Each batch just enqueues the workload, reads results, and updates state
        // ═══════════════════════════════════════════════════════════════════════
        std::string accumulated_output;

        for (int batch = 0; batch < num_batches; batch++) {
            std::cout << "--- Batch " << (batch + 1) << "/" << num_batches << " ---" << std::endl;

            // REUSE the workload (key to avoiding hangs!)
            // This is the pattern from zork_repl.cpp:execute_batch()
            distributed::EnqueueMeshWorkload(cq, workload, /*blocking=*/false);
            distributed::Finish(cq);

            // Read output from this batch
            std::vector<char> output_data(MAX_OUTPUT_SIZE);
            distributed::EnqueueReadMeshBuffer(cq, output_data, output_buffer, /*blocking=*/true);

            // STATE PERSISTENCE NOT VIABLE - See kernel code analysis
            // The issue is the KERNEL's NoC operations (lines 1259-1261, 1333-1334)
            // not the host reading the buffer. Even with STATE_DRAM_ADDR defined
            // just once (not between batches), batch 2+ hangs.
            // Root cause: noc_async_read/write of state buffer causes hang

            // Accumulate output
            accumulated_output += output_data.data();

            std::cout << "Batch " << (batch + 1) << " complete!" << std::endl;
            std::cout << std::endl;
        }

        // Save state to file for next run
        std::ofstream out_state_file(STATE_FILE, std::ios::binary);
        if (out_state_file) {
            out_state_file.write(reinterpret_cast<const char*>(state_data.data()), MAX_STATE_SIZE);
            out_state_file.close();
            std::cout << "[Host] Saved state to " << STATE_FILE << " for next run" << std::endl;
        } else {
            std::cerr << "[Host] Warning: Failed to save state file" << std::endl;
        }

        std::cout << "All batches complete!" << std::endl;

        // Display accumulated output!
        std::cout << std::endl;
        std::cout << "╔════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║  ACCUMULATED ZORK OUTPUT FROM RISC-V              ║" << std::endl;
        std::cout << "╠════════════════════════════════════════════════════╣" << std::endl;
        std::cout << accumulated_output << std::endl;
        std::cout << "╚════════════════════════════════════════════════════╝" << std::endl;

        // Cleanup (close submesh first, then parent mesh)
        std::cout << "[Host] Closing devices..." << std::endl;
        mesh_device->close();
        parent_mesh->close();

        std::cout << std::endl;
        std::cout << "✓ Proof of concept: Successfully loaded game data onto Blackhole!" << std::endl;
        std::cout << "  Next steps: Implement kernel and I/O adapters" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
