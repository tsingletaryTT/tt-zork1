// test_full_structure + buffer allocation
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

constexpr const char* DEFAULT_GAME_FILE = "game/zork1.z3";
constexpr uint32_t MAX_GAME_SIZE = 128 * 1024;
constexpr uint32_t MAX_INPUT_SIZE = 1024;
constexpr uint32_t MAX_OUTPUT_SIZE = 16 * 1024;
constexpr CoreCoord ZORK_CORE = {0, 0};

std::vector<uint8_t> load_game_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open game file: " + filename);
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Could not read game file");
    }
    std::cout << "Loaded game file: " << filename << " (" << size << " bytes)" << std::endl;
    return buffer;
}

int main(int argc, char** argv) {
    std::cout << "=== TEST: Device Init + Buffer Allocation ===" << std::endl;
    const std::string game_file = (argc > 1) ? argv[1] : DEFAULT_GAME_FILE;

    try {
        std::cout << "[Host] Loading game file..." << std::endl;
        std::vector<uint8_t> game_data = load_game_file(game_file);

        std::cout << "[Host] Initializing Blackhole device..." << std::endl;
        const auto system_mesh_shape = distributed::SystemMesh::instance().shape();
        std::cout << "[Host] System mesh shape: " << system_mesh_shape << std::endl;

        std::cout << "[Host] Creating parent mesh..." << std::flush;
        std::shared_ptr<distributed::MeshDevice> parent_mesh =
            distributed::MeshDevice::create(distributed::MeshDeviceConfig(system_mesh_shape));
        std::cout << " done" << std::endl;

        std::cout << "[Host] Creating 1x1 submesh..." << std::flush;
        std::shared_ptr<distributed::MeshDevice> mesh_device =
            parent_mesh->create_submesh(distributed::MeshShape(1, 1));
        std::cout << " done" << std::endl;

        distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();

        // NEW: Add buffer allocation
        std::cout << "[Host] Allocating DRAM buffers..." << std::flush;
        distributed::DeviceLocalBufferConfig dram_config{
            .page_size = 1024,
            .buffer_type = BufferType::DRAM
        };

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
        std::cout << " done" << std::endl;

        // NEW: Upload game data
        std::cout << "[Host] Uploading game data..." << std::flush;
        EnqueueWriteMeshBuffer(cq, game_buffer, game_data, true);
        std::cout << " done" << std::endl;

        std::cout << "\n✅ SUCCESS! Buffer allocation works!" << std::endl;

        mesh_device->close();
        parent_mesh->close();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
