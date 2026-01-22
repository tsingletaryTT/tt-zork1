// Test if we can initialize device, close it, and initialize again
#include <iostream>
#include <memory>
#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device.hpp>
#include <tt-metalium/distributed.hpp>

using namespace tt;
using namespace tt::tt_metal;

int main() {
    std::cout << "Test: Double device initialization" << std::endl;

    try {
        // First initialization
        std::cout << "[1] First device init..." << std::flush;
        auto mesh_device1 = distributed::MeshDevice::create_unit_mesh(0);
        std::cout << " done" << std::endl;

        std::cout << "[2] Closing device..." << std::flush;
        mesh_device1->close();
        std::cout << " done" << std::endl;

        // Second initialization
        std::cout << "[3] Second device init..." << std::flush;
        auto mesh_device2 = distributed::MeshDevice::create_unit_mesh(0);
        std::cout << " done" << std::endl;

        std::cout << "[4] Closing device..." << std::flush;
        mesh_device2->close();
        std::cout << " done" << std::endl;

        std::cout << "✅ SUCCESS: Double init works!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "❌ ERROR: " << e.what() << std::endl;
        return 1;
    }
}
