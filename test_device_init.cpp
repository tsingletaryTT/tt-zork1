// Minimal test to verify TT-Metal device initialization
#include <iostream>
#include <memory>
#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device.hpp>
#include <tt-metalium/distributed.hpp>
#include <tt-metalium/system_mesh.hpp>

using namespace tt;
using namespace tt::tt_metal;

int main() {
    std::cout << "Testing basic TT-Metal device initialization..." << std::endl;

    try {
        // Get the full system mesh shape
        const auto system_mesh_shape = distributed::SystemMesh::instance().shape();
        std::cout << "System mesh shape: " << system_mesh_shape << std::endl;

        // Create mesh device for the full system (recent TT-Metal requires this)
        std::cout << "Creating parent mesh device..." << std::endl;
        std::shared_ptr<distributed::MeshDevice> parent_mesh =
            distributed::MeshDevice::create(distributed::MeshDeviceConfig(system_mesh_shape));

        // Create a 1x1 submesh to use just one device
        std::cout << "Creating 1x1 submesh..." << std::endl;
        std::shared_ptr<distributed::MeshDevice> mesh_device =
            parent_mesh->create_submesh(distributed::MeshShape(1, 1));

        std::cout << "SUCCESS! Device initialized." << std::endl;

        // Cleanup
        mesh_device->close();
        parent_mesh->close();
        std::cout << "Devices closed successfully." << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }
}
