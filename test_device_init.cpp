// Minimal test to verify TT-Metal device initialization
#include <iostream>
#include <memory>
#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device.hpp>
#include <tt-metalium/distributed.hpp>

using namespace tt;
using namespace tt::tt_metal;

int main() {
    std::cout << "Testing basic TT-Metal device initialization..." << std::endl;

    try {
        std::cout << "Creating mesh device..." << std::endl;
        std::shared_ptr<distributed::MeshDevice> mesh_device =
            distributed::MeshDevice::create_unit_mesh(0);

        std::cout << "SUCCESS! Device initialized." << std::endl;

        mesh_device->close();
        std::cout << "Device closed successfully." << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }
}
