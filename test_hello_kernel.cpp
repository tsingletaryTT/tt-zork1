// Test RISC-V kernel execution with minimal "hello world" kernel
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>
#include <string>

#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device.hpp>
#include "tt-metalium/buffer.hpp"
#include <tt-metalium/distributed.hpp>
#include <tt-metalium/system_mesh.hpp>

using namespace tt;
using namespace tt::tt_metal;

constexpr uint32_t MAX_OUTPUT_SIZE = 1024;
constexpr CoreCoord TEST_CORE = {0, 0};

int main() {
    std::cout << "=== TEST: Minimal RISC-V Kernel Execution ===" << std::endl;

    try {
        // Device init - use device 0 only
        std::cout << "[1/7] Initializing device 0..." << std::flush;
        auto mesh_device = distributed::MeshDevice::create_unit_mesh(0);
        distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();
        std::cout << " done" << std::endl;

        // Create output buffer
        std::cout << "[2/7] Allocating output buffer..." << std::flush;
        distributed::DeviceLocalBufferConfig dram_config{
            .page_size = 1024,
            .buffer_type = BufferType::DRAM
        };
        auto output_buffer = distributed::MeshBuffer::create(
            distributed::ReplicatedBufferConfig{.size = MAX_OUTPUT_SIZE},
            dram_config,
            mesh_device.get()
        );
        std::cout << " done" << std::endl;

        // Create program
        std::cout << "[3/7] Creating program..." << std::flush;
        Program program = CreateProgram();
        std::cout << " done" << std::endl;

        // Create simplest possible kernel with address define
        std::cout << "[4/7] Creating ultra-minimal kernel..." << std::flush;

        std::map<std::string, std::string> defines;
        char addr_buf[32];
        snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)output_buffer->address());
        defines["OUTPUT_DRAM_ADDR"] = addr_buf;

        KernelHandle kernel_id = CreateKernel(
            program,
            "/home/ttuser/tt-zork1/kernels/hello_riscv.cpp",
            TEST_CORE,
            DataMovementConfig{
                .processor = DataMovementProcessor::RISCV_0,
                .noc = NOC::RISCV_0_default,
                .defines = defines
            }
        );
        std::cout << " done" << std::endl;

        // No args whatsoever!
        std::cout << "[5/7] No args needed..." << std::flush;
        std::cout << " done" << std::endl;

        // Execute kernel
        std::cout << "[6/7] Executing kernel on RISC-V..." << std::flush;
        distributed::MeshWorkload workload;
        distributed::MeshCoordinateRange device_range =
            distributed::MeshCoordinateRange(mesh_device->shape());
        workload.add_program(device_range, std::move(program));
        distributed::EnqueueMeshWorkload(cq, workload, false);  // non-blocking
        distributed::Finish(cq);  // Wait for completion
        std::cout << " done" << std::endl;

        // Read output
        std::cout << "[7/7] Reading output..." << std::flush;
        std::vector<uint8_t> output_data(MAX_OUTPUT_SIZE);
        EnqueueReadMeshBuffer(cq, output_data, output_buffer, true);
        std::cout << " done" << std::endl;

        // Display result
        std::cout << "\n✅ SUCCESS! Kernel executed!" << std::endl;
        std::cout << "\nOutput from RISC-V:" << std::endl;
        std::cout << "-------------------" << std::endl;
        std::cout << reinterpret_cast<const char*>(output_data.data()) << std::endl;
        std::cout << "-------------------" << std::endl;

        // Cleanup
        mesh_device->close();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n❌ ERROR: " << e.what() << std::endl;
        return 1;
    }
}
