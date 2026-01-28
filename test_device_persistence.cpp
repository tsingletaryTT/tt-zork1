// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * test_device_persistence.cpp - Prove device can execute multiple kernels
 *
 * This test demonstrates the device persistence pattern:
 * 1. Device opens ONCE
 * 2. Execute N kernels in sequence
 * 3. Device stays open (no close between kernels)
 * 4. Device closes ONCE at end
 *
 * This proves the pattern works for the Z-machine interpreter integration.
 */

#include <iostream>
#include <memory>
#include <vector>
#include <string>

#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device.hpp>
#include <tt-metalium/buffer.hpp>
#include <tt-metalium/distributed.hpp>

using namespace tt;
using namespace tt::tt_metal;

constexpr tt::tt_metal::CoreCoord TEST_CORE = {0, 0};
constexpr size_t OUTPUT_SIZE = 1024;

int main(int argc, char* argv[]) {
    const int num_runs = (argc > 1) ? atoi(argv[1]) : 3;  // Default: 3 runs

    std::cout << "╔══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  Device Persistence Test - Multiple Kernel Executions   ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    std::cout << "Testing: " << num_runs << " consecutive kernel executions" << std::endl;
    std::cout << std::endl;

    try {
        // ═══════════════════════════════════════════════════════
        // STEP 1: DEVICE INITIALIZATION (ONCE!)
        // ═══════════════════════════════════════════════════════
        std::cout << "[1/4] Initializing device (ONCE at start)..." << std::endl;

        auto mesh_device = distributed::MeshDevice::create_unit_mesh(0);
        distributed::MeshCommandQueue& cq = mesh_device->mesh_command_queue();

        // Enable program cache for repeated execution
        auto devices = mesh_device->get_devices();
        for (auto device : devices) {
            device->enable_program_cache();
        }

        std::cout << "      ✅ Device initialized and program cache enabled" << std::endl;
        std::cout << std::endl;

        // ═══════════════════════════════════════════════════════
        // STEP 2: BUFFER ALLOCATION (ONCE!)
        // ═══════════════════════════════════════════════════════
        std::cout << "[2/4] Allocating DRAM buffer (ONCE)..." << std::endl;

        distributed::DeviceLocalBufferConfig dram_config{
            .page_size = OUTPUT_SIZE,
            .buffer_type = BufferType::DRAM
        };

        auto output_buffer = distributed::MeshBuffer::create(
            distributed::ReplicatedBufferConfig{.size = OUTPUT_SIZE},
            dram_config,
            mesh_device.get()
        );

        std::cout << "      ✅ DRAM buffer allocated at 0x" << std::hex
                  << output_buffer->address() << std::dec << std::endl;
        std::cout << std::endl;

        // ═══════════════════════════════════════════════════════
        // STEP 3: EXECUTE MULTIPLE KERNELS (DEVICE STAYS OPEN!)
        // ═══════════════════════════════════════════════════════
        std::cout << "[3/4] Executing " << num_runs << " kernels in sequence..." << std::endl;
        std::cout << std::endl;

        for (int run = 0; run < num_runs; run++) {
            std::cout << "  Run " << (run + 1) << "/" << num_runs << ":" << std::endl;

            // Create program (each kernel gets fresh program)
            Program program = CreateProgram();

            // Pass buffer address to kernel
            std::map<std::string, std::string> kernel_defines;
            char addr_str[32];
            snprintf(addr_str, sizeof(addr_str), "0x%lx", (unsigned long)output_buffer->address());
            kernel_defines["OUTPUT_DRAM_ADDR"] = addr_str;

            // Create kernel
            KernelHandle kernel_id = CreateKernel(
                program,
                "/home/ttuser/code/tt-zork1/kernels/test_pinned_output.cpp",
                TEST_CORE,
                DataMovementConfig{
                    .processor = DataMovementProcessor::RISCV_0,
                    .noc = NOC::RISCV_0_default,
                    .defines = kernel_defines
                }
            );

            std::cout << "    - Creating kernel... done" << std::endl;

            // Execute kernel
            distributed::MeshWorkload workload;
            distributed::MeshCoordinateRange device_range =
                distributed::MeshCoordinateRange(mesh_device->shape());
            workload.add_program(device_range, std::move(program));

            std::cout << "    - Executing... " << std::flush;
            distributed::EnqueueMeshWorkload(cq, workload, false);
            distributed::Finish(cq);
            std::cout << "done" << std::endl;

            // Read output
            std::vector<uint8_t> output_data(OUTPUT_SIZE);
            distributed::EnqueueReadMeshBuffer(cq, output_data, output_buffer, true);

            std::cout << "    - Reading output... done" << std::endl;
            std::cout << "    ✅ Run " << (run + 1) << " complete!" << std::endl;
            std::cout << std::endl;

            // Show first line of output from first run
            if (run == 0) {
                const char* output = reinterpret_cast<const char*>(output_data.data());
                std::cout << "    Sample output: \"" << std::string(output, 60) << "...\"" << std::endl;
                std::cout << std::endl;
            }
        }

        std::cout << "      ✅ All " << num_runs << " kernels executed successfully!" << std::endl;
        std::cout << "      🎉 Device persistence PROVEN!" << std::endl;
        std::cout << std::endl;

        // ═══════════════════════════════════════════════════════
        // STEP 4: CLEANUP (CLOSE ONCE!)
        // ═══════════════════════════════════════════════════════
        std::cout << "[4/4] Closing device (ONCE at end)..." << std::endl;
        mesh_device->close();
        std::cout << "      ✅ Device closed" << std::endl;
        std::cout << std::endl;

        std::cout << "╔════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║  SUCCESS: Device Persistence Pattern Works!           ║" << std::endl;
        std::cout << "╠════════════════════════════════════════════════════════╣" << std::endl;
        std::cout << "║  Pattern Verified:                                     ║" << std::endl;
        std::cout << "║    1. Device opens ONCE                                ║" << std::endl;
        std::cout << "║    2. Buffers allocated ONCE                           ║" << std::endl;
        std::cout << "║    3. Multiple kernels execute                         ║" << std::endl;
        std::cout << "║    4. Device closes ONCE                               ║" << std::endl;
        std::cout << "║                                                        ║" << std::endl;
        std::cout << "║  ✅ Ready for Z-machine integration!                   ║" << std::endl;
        std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << std::endl;
        std::cerr << "❌ ERROR: " << e.what() << std::endl;
        return 1;
    }
}
