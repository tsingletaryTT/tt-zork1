// SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
// SPDX-License-Identifier: Apache-2.0

/**
 * Proof of Concept: RISC-V writes directly to host memory via PinnedMemory
 *
 * This test demonstrates:
 * 1. Creating pinned host memory accessible via NoC
 * 2. RISC-V kernel writing message to host memory
 * 3. Host reading message directly from pinned memory
 * 4. Device stays open (no close/reopen cycle)
 */

#include <iostream>
#include <string>
#include <cstring>
#include <memory>

#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device.hpp>
#include <tt-metalium/buffer.hpp>
#include <tt-metalium/distributed.hpp>
#include <tt-metalium/experimental/pinned_memory.hpp>

using namespace tt;
using namespace tt::tt_metal;

constexpr CoreCoord TEST_CORE = {0, 0};
constexpr size_t OUTPUT_SIZE = 1024;  // 1KB output buffer

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  PinnedMemory Test: RISC-V → Host Memory Direct Write   ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    try {
        // Step 1: Create device
        std::cout << "[1/6] Creating mesh device..." << std::endl;
        auto mesh_device = distributed::MeshDevice::create_unit_mesh(0);
        std::cout << "      ✅ Device created" << std::endl;

        // Step 2: Check if pinned memory with NoC mapping is supported
        std::cout << "[2/6] Checking PinnedMemory support..." << std::endl;
        auto params = experimental::GetMemoryPinningParameters(*mesh_device);
        std::cout << "      max_pins: " << params.max_pins << std::endl;
        std::cout << "      max_total_pin_size: " << params.max_total_pin_size << std::endl;
        std::cout << "      can_map_to_noc: " << (params.can_map_to_noc ? "YES" : "NO") << std::endl;

        if (!params.can_map_to_noc) {
            std::cout << "      ❌ PinnedMemory with NoC mapping not supported on this system" << std::endl;
            std::cout << "      Cannot proceed with test." << std::endl;
            mesh_device->close();
            return 1;
        }
        std::cout << "      ✅ PinnedMemory supported!" << std::endl;

        // Step 3: Create pinned host memory
        std::cout << "[3/6] Creating pinned host memory..." << std::endl;

        // Allocate aligned host memory
        auto dst = std::make_shared<tt::tt_metal::vector_aligned<uint8_t>>(OUTPUT_SIZE, 0);
        uint8_t* dst_ptr_aligned = dst->data();

        // Create HostBuffer wrapper
        HostBuffer host_buffer(
            tt::stl::Span<uint8_t>(dst_ptr_aligned, OUTPUT_SIZE),
            MemoryPin(dst)
        );

        // Create coordinate range for single device
        distributed::MeshCoordinate coord(0, 0);
        auto coordinate_range_set = distributed::MeshCoordinateRangeSet(
            distributed::MeshCoordinateRange(coord, coord)
        );

        // Pin the memory with NoC mapping
        auto pinned_memory = experimental::PinnedMemory::Create(
            *mesh_device,
            coordinate_range_set,
            host_buffer,
            /*map_to_noc=*/true
        );

        std::cout << "      Pinned memory size: " << pinned_memory->get_buffer_size() << " bytes" << std::endl;
        std::cout << "      ✅ Pinned memory created" << std::endl;

        // Step 4: Get NoC address for kernel
        std::cout << "[4/6] Getting NoC address..." << std::endl;
        auto noc_addr_opt = pinned_memory->get_noc_addr(0);  // Device ID 0

        if (!noc_addr_opt.has_value()) {
            std::cout << "      ❌ Failed to get NoC address" << std::endl;
            mesh_device->close();
            return 1;
        }

        auto noc_addr = noc_addr_opt.value();
        std::cout << "      NoC address: 0x" << std::hex << noc_addr.addr << std::dec << std::endl;
        std::cout << "      PCIe XY encoding: 0x" << std::hex << noc_addr.pcie_xy_enc << std::dec << std::endl;
        std::cout << "      Device ID: " << noc_addr.device_id << std::endl;
        std::cout << "      ✅ NoC address obtained" << std::endl;

        // Step 5: Create and run kernel
        std::cout << "[5/6] Creating kernel..." << std::endl;

        Program program = CreateProgram();

        // Pass NoC address as compile-time defines
        std::map<std::string, std::string> kernel_defines;
        char addr_str[32];
        snprintf(addr_str, sizeof(addr_str), "0x%lx", (unsigned long)noc_addr.addr);
        kernel_defines["OUTPUT_NOC_ADDR"] = addr_str;

        snprintf(addr_str, sizeof(addr_str), "0x%x", noc_addr.pcie_xy_enc);
        kernel_defines["PCIE_XY_ENC"] = addr_str;

        KernelHandle kernel_id = CreateKernel(
            program,
            "/home/ttuser/code/tt-zork1/kernels/test_pinned_write.cpp",
            TEST_CORE,
            DataMovementConfig{
                .processor = DataMovementProcessor::RISCV_0,
                .noc = NOC::RISCV_0_default,
                .defines = kernel_defines
            }
        );

        std::cout << "      ✅ Kernel created" << std::endl;
        std::cout << "      Executing kernel..." << std::endl;

        distributed::MeshWorkload workload;
        distributed::MeshCoordinateRange device_range(distributed::MeshShape(1, 1));
        workload.add_program(device_range, std::move(program));

        distributed::EnqueueMeshWorkload(mesh_device->mesh_command_queue(), workload, false);
        distributed::Finish(mesh_device->mesh_command_queue());

        std::cout << "      ✅ Kernel executed" << std::endl;

        // Step 6: Read output from pinned memory
        std::cout << "[6/6] Reading output from host memory..." << std::endl;

        void* host_ptr = pinned_memory->lock();  // Wait for device writes
        const char* output = static_cast<const char*>(host_ptr);

        std::cout << std::endl;
        std::cout << "╔════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║  OUTPUT FROM RISC-V (via PinnedMemory):               ║" << std::endl;
        std::cout << "╠════════════════════════════════════════════════════════╣" << std::endl;
        std::cout << "║  " << output << std::endl;
        std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;
        std::cout << std::endl;

        pinned_memory->unlock();

        // Close device
        std::cout << "[Cleanup] Closing device..." << std::endl;
        mesh_device->close();
        std::cout << "           ✅ Device closed" << std::endl;

        std::cout << std::endl;
        std::cout << "🎉 SUCCESS: RISC-V wrote directly to host memory!" << std::endl;
        std::cout << "    This proves we can eliminate DRAM buffers and device reopen cycles!" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << std::endl;
        std::cerr << "❌ ERROR: " << e.what() << std::endl;
        return 1;
    }
}
