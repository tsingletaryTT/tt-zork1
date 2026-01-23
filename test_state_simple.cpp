// Test state persistence with simple data (just counters)
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device.hpp>
#include <tt-metalium/buffer.hpp>
#include <tt-metalium/distributed.hpp>

using namespace tt;
using namespace tt::tt_metal;

constexpr uint32_t STATE_SIZE = 16 * 1024;
constexpr CoreCoord TEST_CORE = {0, 0};
const char* STATE_FILE = "/tmp/test_state.bin";

int main() {
    std::cout << "=== Simple State Persistence Test ===" << std::endl;

    try {
        // Load or create simple state (just a counter)
        std::vector<uint8_t> state_data(STATE_SIZE, 0);
        uint32_t* counter = reinterpret_cast<uint32_t*>(state_data.data());

        std::ifstream in_file(STATE_FILE, std::ios::binary);
        if (in_file) {
            in_file.read(reinterpret_cast<char*>(state_data.data()), STATE_SIZE);
            in_file.close();
            std::cout << "Loaded previous state, counter = " << *counter << std::endl;
            (*counter)++;  // Increment
        } else {
            *counter = 1;  // Start at 1
            std::cout << "Starting fresh, counter = " << *counter << std::endl;
        }

        // Initialize device
        std::cout << "Initializing device..." << std::flush;
        auto mesh_device = distributed::MeshDevice::create_unit_mesh(0);
        distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();
        std::cout << " done" << std::endl;

        // Create state buffer
        distributed::DeviceLocalBufferConfig state_config{
            .page_size = STATE_SIZE,
            .buffer_type = BufferType::DRAM
        };
        auto state_buffer = distributed::MeshBuffer::create(
            distributed::ReplicatedBufferConfig{.size = STATE_SIZE},
            state_config,
            mesh_device.get()
        );

        // Upload state
        std::cout << "Uploading state (counter=" << *counter << ")..." << std::flush;
        EnqueueWriteMeshBuffer(cq, state_buffer, state_data, true);
        std::cout << " done" << std::endl;

        // Create simple kernel that reads state and increments counter
        std::cout << "Creating kernel..." << std::flush;
        Program program = CreateProgram();

        std::map<std::string, std::string> defines;
        char addr_buf[32];
        snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)state_buffer->address());
        defines["STATE_DRAM_ADDR"] = addr_buf;

        KernelHandle kernel = CreateKernel(
            program,
            "/home/ttuser/tt-zork1/kernels/test_simple_state.cpp",
            TEST_CORE,
            DataMovementConfig{
                .processor = DataMovementProcessor::RISCV_0,
                .noc = NOC::RISCV_0_default,
                .defines = defines
            }
        );
        std::cout << " done" << std::endl;

        // Execute
        std::cout << "Executing kernel..." << std::flush;
        distributed::MeshWorkload workload;
        distributed::MeshCoordinateRange device_range(mesh_device->shape());
        workload.add_program(device_range, std::move(program));
        distributed::EnqueueMeshWorkload(cq, workload, false);
        distributed::Finish(cq);
        std::cout << " done" << std::endl;

        // Read state back
        std::cout << "Reading state back..." << std::flush;
        EnqueueReadMeshBuffer(cq, state_data, state_buffer, true);
        std::cout << " done" << std::endl;

        std::cout << "Counter after kernel: " << *counter << std::endl;

        // Save state
        std::ofstream out_file(STATE_FILE, std::ios::binary);
        out_file.write(reinterpret_cast<const char*>(state_data.data()), STATE_SIZE);
        out_file.close();
        std::cout << "Saved state to " << STATE_FILE << std::endl;

        mesh_device->close();
        std::cout << "✅ Test complete!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "❌ ERROR: " << e.what() << std::endl;
        return 1;
    }
}
