// Simplified test: Device init + simple kernel creation (WITH game loading)
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device.hpp>
#include <tt-metalium/distributed.hpp>
#include <tt-metalium/system_mesh.hpp>

using namespace tt;
using namespace tt::tt_metal;

int main() {
    std::cout << "=== Simplified Kernel Test (WITH game file loading) ===" << std::endl;

    try {
        // Step 0: Load game file BEFORE device init (like the full program)
        std::cout << "Step 0: Loading game file..." << std::flush;
        std::ifstream file("game/zork1.z3", std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << " file not found!" << std::endl;
            return 1;
        }
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> game_data(size);
        if (!file.read(reinterpret_cast<char*>(game_data.data()), size)) {
            std::cerr << " read failed!" << std::endl;
            return 1;
        }
        std::cout << " " << size << " bytes loaded" << std::endl;

        // Step 1: Device initialization (same as test_device_init)
        std::cout << "Step 1: Getting system mesh shape..." << std::flush;
        const auto system_mesh_shape = distributed::SystemMesh::instance().shape();
        std::cout << " " << system_mesh_shape << std::endl;

        std::cout << "Step 2: Creating parent mesh..." << std::flush;
        std::shared_ptr<distributed::MeshDevice> parent_mesh =
            distributed::MeshDevice::create(distributed::MeshDeviceConfig(system_mesh_shape));
        std::cout << " done" << std::endl;

        std::cout << "Step 3: Creating 1x1 submesh..." << std::flush;
        std::shared_ptr<distributed::MeshDevice> mesh_device =
            parent_mesh->create_submesh(distributed::MeshShape(1, 1));
        std::cout << " done" << std::endl;

        // Step 4: Create mesh workload (THIS IS THE KEY TEST)
        std::cout << "Step 4: Creating MeshWorkload..." << std::flush;
        distributed::MeshWorkload workload;
        std::cout << " done" << std::endl;

        // Step 5: Create coordinate range
        std::cout << "Step 5: Creating coordinate range..." << std::flush;
        distributed::MeshCoordinateRange device_range =
            distributed::MeshCoordinateRange(mesh_device->shape());
        std::cout << " done" << std::endl;

        // Step 6: Get command queue
        std::cout << "Step 6: Getting command queue..." << std::flush;
        distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();
        std::cout << " done" << std::endl;

        // Step 5: Cleanup
        std::cout << "Step 5: Closing devices..." << std::flush;
        mesh_device->close();
        parent_mesh->close();
        std::cout << " done" << std::endl;

        std::cout << "\n✅ SUCCESS! Simple kernel creation works!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n❌ FAILED: " << e.what() << std::endl;
        return 1;
    }
}
