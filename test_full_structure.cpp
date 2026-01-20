// Test with EXACT same structure as full program, but only device init
// This will help identify what triggers the hang

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

// Game file path (SAME AS FULL PROGRAM)
constexpr const char* DEFAULT_GAME_FILE = "game/zork1.z3";

// Buffer sizes (SAME AS FULL PROGRAM)
constexpr uint32_t MAX_GAME_SIZE = 128 * 1024;
constexpr uint32_t MAX_INPUT_SIZE = 1024;
constexpr uint32_t MAX_OUTPUT_SIZE = 16 * 1024;

// Core to run kernel on (SAME AS FULL PROGRAM - includes deprecated CoreCoord)
constexpr CoreCoord ZORK_CORE = {0, 0};

// Load game file function (SAME AS FULL PROGRAM)
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
    // EXACT same header as full program
    std::cout << "╔══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  ZORK I on Tenstorrent Blackhole RISC-V Cores" << std::endl;
    std::cout << "║  1977 Game on 2026 AI Accelerator" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    const std::string game_file = (argc > 1) ? argv[1] : DEFAULT_GAME_FILE;

    try {
        // EXACT same sequence as full program
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

        std::cout << "[Host] Using submesh device at coordinate (0,0)" << std::endl;

        // STOP HERE - don't create anything else
        std::cout << "\n✅ SUCCESS! Device init works with full program structure!" << std::endl;

        // Cleanup
        std::cout << "[Host] Closing devices..." << std::endl;
        mesh_device->close();
        parent_mesh->close();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
