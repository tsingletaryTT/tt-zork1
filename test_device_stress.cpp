#include <iostream>
#include <tt-metalium/host_api.hpp>
#include <tt-metalium/distributed.hpp>

int main() {
    for (int i = 0; i < 10; i++) {
        try {
            std::cout << "\n=== Cycle " << (i+1) << " ===" << std::endl;
            
            auto shape = tt::tt_metal::distributed::SystemMesh::instance().shape();
            std::cout << "Detected shape: " << shape << std::endl;
            
            // Try to create parent mesh
            auto parent_mesh = tt::tt_metal::distributed::MeshDevice::create(
                tt::tt_metal::distributed::MeshDeviceConfig(shape));
            std::cout << "Parent mesh created successfully" << std::endl;
            
            // Immediately close
            parent_mesh.reset();
            std::cout << "Parent mesh closed" << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "ERROR: " << e.what() << std::endl;
        }
    }
    return 0;
}
