// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * Python bindings for Zork on Blackhole
 *
 * This module provides Python access to the Z-machine interpreter running
 * on Tenstorrent Blackhole RISC-V cores.
 *
 * Usage:
 *   import zork_tt
 *   device = zork_tt.init_device()
 *   zork_tt.load_game(device, "game/zork1.z3")
 *   output = zork_tt.execute_batch(device, 100)
 *   zork_tt.close_device(device)
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>

#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device.hpp>
#include "tt-metalium/buffer.hpp"
#include <tt-metalium/distributed.hpp>

namespace py = pybind11;
using namespace tt;
using namespace tt::tt_metal;

// Buffer sizes
constexpr uint32_t MAX_GAME_SIZE = 128 * 1024;
constexpr uint32_t MAX_OUTPUT_SIZE = 16 * 1024;
constexpr uint32_t MAX_STATE_SIZE = 16 * 1024;
constexpr CoreCoord ZORK_CORE = {0, 0};

/**
 * Device context - holds persistent state
 */
struct ZorkDevice {
    std::shared_ptr<distributed::MeshDevice> mesh_device;
    std::shared_ptr<distributed::MeshBuffer> game_buffer;
    std::shared_ptr<distributed::MeshBuffer> output_buffer;
    std::shared_ptr<distributed::MeshBuffer> state_buffer;
    distributed::MeshCommandQueue* command_queue;
    std::vector<uint8_t> state_data;

    ZorkDevice() : command_queue(nullptr) {
        state_data.resize(MAX_STATE_SIZE, 0);
    }
};

/**
 * Initialize device and allocate buffers
 * Returns opaque device handle (as integer for Python)
 */
uintptr_t init_device() {
    auto device = std::make_unique<ZorkDevice>();

    // Create mesh device
    device->mesh_device = distributed::MeshDevice::create_unit_mesh(0);

    // Enable program cache - critical for performance!
    auto devices = device->mesh_device->get_devices();
    for (auto dev : devices) {
        dev->enable_program_cache();
    }

    // Get command queue
    device->command_queue = &device->mesh_device->mesh_command_queue();

    // Allocate DRAM buffers
    distributed::DeviceLocalBufferConfig game_dram_config{
        .page_size = MAX_GAME_SIZE,
        .buffer_type = BufferType::DRAM
    };

    distributed::DeviceLocalBufferConfig output_dram_config{
        .page_size = MAX_OUTPUT_SIZE,
        .buffer_type = BufferType::DRAM
    };

    distributed::DeviceLocalBufferConfig state_dram_config{
        .page_size = MAX_STATE_SIZE,
        .buffer_type = BufferType::DRAM
    };

    device->game_buffer = distributed::MeshBuffer::create(
        distributed::ReplicatedBufferConfig{.size = MAX_GAME_SIZE},
        game_dram_config,
        device->mesh_device.get()
    );

    device->output_buffer = distributed::MeshBuffer::create(
        distributed::ReplicatedBufferConfig{.size = MAX_OUTPUT_SIZE},
        output_dram_config,
        device->mesh_device.get()
    );

    device->state_buffer = distributed::MeshBuffer::create(
        distributed::ReplicatedBufferConfig{.size = MAX_STATE_SIZE},
        state_dram_config,
        device->mesh_device.get()
    );

    // Return as opaque handle
    return reinterpret_cast<uintptr_t>(device.release());
}

/**
 * Load game file into device DRAM
 */
void load_game(uintptr_t device_handle, const std::string& filename) {
    auto* device = reinterpret_cast<ZorkDevice*>(device_handle);

    // Load game file
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open game file: " + filename);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size > MAX_GAME_SIZE) {
        throw std::runtime_error("Game file too large");
    }

    std::vector<uint8_t> game_data(MAX_GAME_SIZE, 0);
    if (!file.read(reinterpret_cast<char*>(game_data.data()), size)) {
        throw std::runtime_error("Failed to read game file");
    }

    // Upload to DRAM
    EnqueueWriteMeshBuffer(*device->command_queue, device->game_buffer, game_data, true);

    // Initialize state buffer (zeros for fresh start)
    EnqueueWriteMeshBuffer(*device->command_queue, device->state_buffer, device->state_data, true);
}

/**
 * Execute one batch of Z-machine instructions
 * Returns game output as string
 */
std::string execute_batch(uintptr_t device_handle, int num_instructions) {
    auto* device = reinterpret_cast<ZorkDevice*>(device_handle);

    // Create program
    Program program = CreateProgram();

    // Pass buffer addresses as compile-time defines
    std::map<std::string, std::string> kernel_defines;
    char addr_buf[32];

    snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)device->game_buffer->address());
    kernel_defines["GAME_DRAM_ADDR"] = addr_buf;

    snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)device->output_buffer->address());
    kernel_defines["OUTPUT_DRAM_ADDR"] = addr_buf;

    snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)device->state_buffer->address());
    kernel_defines["STATE_DRAM_ADDR"] = addr_buf;

    // Create kernel
    KernelHandle kernel_id = CreateKernel(
        program,
        "/home/ttuser/code/tt-zork1/kernels/zork_interpreter.cpp",
        ZORK_CORE,
        DataMovementConfig{
            .processor = DataMovementProcessor::RISCV_0,
            .noc = NOC::RISCV_0_default,
            .defines = kernel_defines
        }
    );

    // Execute kernel
    distributed::MeshWorkload workload;
    distributed::MeshCoordinateRange device_range(device->mesh_device->shape());
    workload.add_program(device_range, std::move(program));
    distributed::EnqueueMeshWorkload(*device->command_queue, workload, false);
    distributed::Finish(*device->command_queue);

    // Read output
    std::vector<char> output_data(MAX_OUTPUT_SIZE);
    distributed::EnqueueReadMeshBuffer(*device->command_queue, output_data, device->output_buffer, true);

    // Read state (for next batch)
    distributed::EnqueueReadMeshBuffer(*device->command_queue, device->state_data, device->state_buffer, true);

    // Return output as string
    return std::string(output_data.data());
}

/**
 * Get current Z-machine state
 * Returns state as bytes (for saving to file)
 */
py::bytes get_state(uintptr_t device_handle) {
    auto* device = reinterpret_cast<ZorkDevice*>(device_handle);
    return py::bytes(reinterpret_cast<const char*>(device->state_data.data()),
                     device->state_data.size());
}

/**
 * Set Z-machine state
 * Loads state from bytes (for restoring from file)
 */
void set_state(uintptr_t device_handle, py::bytes state) {
    auto* device = reinterpret_cast<ZorkDevice*>(device_handle);
    std::string state_str = state;

    if (state_str.size() != MAX_STATE_SIZE) {
        throw std::runtime_error("Invalid state size");
    }

    std::copy(state_str.begin(), state_str.end(), device->state_data.begin());

    // Upload to device
    EnqueueWriteMeshBuffer(*device->command_queue, device->state_buffer, device->state_data, true);
}

/**
 * Close device and free resources
 */
void close_device(uintptr_t device_handle) {
    auto* device = reinterpret_cast<ZorkDevice*>(device_handle);

    if (device->mesh_device) {
        device->mesh_device->close();
    }

    delete device;
}

/**
 * PyBind11 module definition
 */
PYBIND11_MODULE(zork_tt, m) {
    m.doc() = "Zork I on Tenstorrent Blackhole - Python bindings";

    m.def("init_device", &init_device,
          "Initialize Blackhole device and allocate buffers");

    m.def("load_game", &load_game,
          "Load game file into device DRAM",
          py::arg("device"), py::arg("filename"));

    m.def("execute_batch", &execute_batch,
          "Execute batch of Z-machine instructions",
          py::arg("device"), py::arg("num_instructions") = 100);

    m.def("get_state", &get_state,
          "Get current Z-machine state as bytes",
          py::arg("device"));

    m.def("set_state", &set_state,
          "Set Z-machine state from bytes",
          py::arg("device"), py::arg("state"));

    m.def("close_device", &close_device,
          "Close device and free resources",
          py::arg("device"));
}
