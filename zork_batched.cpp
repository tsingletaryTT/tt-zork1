// Batched execution of Zork interpreter - runs multiple kernels with state persistence
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>
#include <fstream>
#include <cstring>

#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device.hpp>
#include "tt-metalium/buffer.hpp"
#include <tt-metalium/distributed.hpp>

using namespace tt;
using namespace tt::tt_metal;

constexpr uint32_t MAX_GAME_SIZE = 128 * 1024;
constexpr uint32_t MAX_OUTPUT_SIZE = 16 * 1024;
constexpr uint32_t MAX_STATE_SIZE = 16 * 1024;  // ZMachineState is ~10KB
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

int main(int argc, char** argv) {
    const char* game_file = (argc > 1) ? argv[1] : "game/zork1.z3";
    const int max_batches = (argc > 2) ? atoi(argv[2]) : 10;  // Default: 10 batches

    std::cout << "╔══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  ZORK I - Batched Execution on Blackhole RISC-V        ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    std::cout << "Max batches: " << max_batches << " (each batch = 100 instructions)" << std::endl;
    std::cout << std::endl;

    try {
        // Load game
        std::cout << "[1/6] Loading game file..." << std::flush;
        std::vector<uint8_t> game_data = load_game_file(game_file);
        std::cout << " done" << std::endl;

        // Initialize device
        std::cout << "[2/6] Initializing device 0..." << std::flush;
        auto mesh_device = distributed::MeshDevice::create_unit_mesh(0);
        distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();
        std::cout << " done" << std::endl;

        // Create 3 DRAM buffers: game, output, state
        std::cout << "[3/6] Allocating DRAM buffers..." << std::flush;

        distributed::DeviceLocalBufferConfig game_config{
            .page_size = MAX_GAME_SIZE,
            .buffer_type = BufferType::DRAM
        };
        distributed::DeviceLocalBufferConfig output_config{
            .page_size = MAX_OUTPUT_SIZE,
            .buffer_type = BufferType::DRAM
        };
        distributed::DeviceLocalBufferConfig state_config{
            .page_size = MAX_STATE_SIZE,
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
        auto state_buffer = distributed::MeshBuffer::create(
            distributed::ReplicatedBufferConfig{.size = MAX_STATE_SIZE},
            state_config,
            mesh_device.get()
        );

        std::cout << " done" << std::endl;

        // Upload game data once
        std::cout << "[4/6] Uploading game data..." << std::flush;
        EnqueueWriteMeshBuffer(cq, game_buffer, game_data, true);

        // Initialize state buffer to zeros
        std::vector<uint8_t> state_data(MAX_STATE_SIZE, 0);
        EnqueueWriteMeshBuffer(cq, state_buffer, state_data, true);

        std::cout << " done" << std::endl;

        // Run batches
        std::cout << "[5/6] Running batched execution..." << std::endl;
        std::cout << std::endl;

        std::string accumulated_output;

        for (int batch = 0; batch < max_batches; batch++) {
            std::cout << "  Batch " << (batch + 1) << "/" << max_batches << "..." << std::flush;

            // Create program
            Program program = CreateProgram();

            // Create kernel with all 3 buffer addresses
            std::map<std::string, std::string> defines;
            char addr_buf[32];

            snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)game_buffer->address());
            defines["GAME_DRAM_ADDR"] = addr_buf;

            snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)output_buffer->address());
            defines["OUTPUT_DRAM_ADDR"] = addr_buf;

            snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)state_buffer->address());
            defines["STATE_DRAM_ADDR"] = addr_buf;  // Enable batched mode!

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

            // Read output from this batch
            std::vector<uint8_t> output_data(MAX_OUTPUT_SIZE);
            EnqueueReadMeshBuffer(cq, output_data, output_buffer, true);

            // Read state to check if finished
            EnqueueReadMeshBuffer(cq, state_data, state_buffer, true);

            // Accumulate output
            const char* output_str = reinterpret_cast<const char*>(output_data.data());
            accumulated_output += output_str;

            // Check if game finished (read from state buffer)
            struct ZMachineState {
                uint32_t pc_offset;
                uint32_t sp;
                uint16_t stack[1024];
                uint32_t frame_sp;
                // ... (we only care about finished flag)
                char padding[8000];  // Approximate offset
                bool finished;
            };

            // For now, just check output length as a proxy
            size_t output_len = strlen(output_str);

            std::cout << " done (" << output_len << " chars)" << std::endl;

            // If output is very short, might be finished
            if (output_len < 50 && batch > 0) {
                std::cout << "  (Output seems complete, stopping)" << std::endl;
                break;
            }
        }

        std::cout << std::endl;
        std::cout << "[6/6] Execution complete!" << std::endl;
        std::cout << std::endl;

        // Display accumulated output
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "ACCUMULATED OUTPUT FROM ALL BATCHES:" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << accumulated_output << std::endl;
        std::cout << std::string(70, '=') << std::endl;

        mesh_device->close();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n❌ ERROR: " << e.what() << std::endl;
        return 1;
    }
}
