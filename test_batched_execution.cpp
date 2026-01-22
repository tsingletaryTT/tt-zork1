// Test: Run Zork kernel multiple times in batches to see more output
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>
#include <fstream>

#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device.hpp>
#include "tt-metalium/buffer.hpp"
#include <tt-metalium/distributed.hpp>

using namespace tt;
using namespace tt::tt_metal;

constexpr uint32_t MAX_GAME_SIZE = 128 * 1024;
constexpr uint32_t MAX_OUTPUT_SIZE = 16 * 1024;
constexpr CoreCoord ZORK_CORE = {0, 0};

std::vector<uint8_t> load_game_file(const char* filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error(std::string("Failed to open game file: ") + filename);
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(MAX_GAME_SIZE, 0);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read game file");
    }
    std::cout << "Loaded: " << filename << " (" << size << " bytes)" << std::endl;
    return buffer;
}

int main() {
    std::cout << "=== BATCHED EXECUTION TEST ===" << std::endl;
    std::cout << "Running interpret(100) multiple times to get more output" << std::endl << std::endl;

    try {
        // Load game
        std::vector<uint8_t> game_data = load_game_file("game/zork1.z3");

        // Initialize device
        std::cout << "[1] Initializing device 0..." << std::flush;
        auto mesh_device = distributed::MeshDevice::create_unit_mesh(0);
        distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();
        std::cout << " done" << std::endl;

        // Create buffers with correct page sizes
        std::cout << "[2] Allocating DRAM buffers..." << std::flush;
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
        std::cout << " done" << std::endl;

        // Upload game data ONCE
        std::cout << "[3] Uploading game data..." << std::flush;
        EnqueueWriteMeshBuffer(cq, game_buffer, game_data, true);
        std::cout << " done" << std::endl;

        // Run multiple batches
        constexpr int NUM_BATCHES = 3;
        std::cout << "\n[4] Running " << NUM_BATCHES << " batches of interpret(100)...\n" << std::endl;

        for (int batch = 0; batch < NUM_BATCHES; batch++) {
            std::cout << "  Batch " << (batch + 1) << "/" << NUM_BATCHES << "..." << std::flush;

            // Create program
            Program program = CreateProgram();

            // Create kernel with buffer addresses
            std::map<std::string, std::string> defines;
            char addr_buf[32];
            snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)game_buffer->address());
            defines["GAME_DRAM_ADDR"] = addr_buf;
            snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)output_buffer->address());
            defines["OUTPUT_DRAM_ADDR"] = addr_buf;

            KernelHandle kernel_id = CreateKernel(
                program,
                "/home/ttuser/tt-zork1/kernels/zork_interpreter.cpp",
                ZORK_CORE,
                DataMovementConfig{
                    .processor = DataMovementProcessor::RISCV_0,
                    .noc = NOC::RISCV_0_default,
                    .defines = defines
                }
            );

            // Execute
            distributed::MeshWorkload workload;
            distributed::MeshCoordinateRange device_range(mesh_device->shape());
            workload.add_program(device_range, std::move(program));
            distributed::EnqueueMeshWorkload(cq, workload, false);
            distributed::Finish(cq);

            std::cout << " done" << std::endl;
        }

        // Read final output
        std::cout << "\n[5] Reading accumulated output..." << std::flush;
        std::vector<uint8_t> output_data(MAX_OUTPUT_SIZE);
        EnqueueReadMeshBuffer(cq, output_data, output_buffer, true);
        std::cout << " done" << std::endl;

        // Display output
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "ACCUMULATED OUTPUT FROM " << NUM_BATCHES << " BATCHES:" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        std::cout << output_data.data() << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        mesh_device->close();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n❌ ERROR: " << e.what() << std::endl;
        return 1;
    }
}
