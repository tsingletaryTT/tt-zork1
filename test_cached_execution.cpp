// Test if enabling program cache helps with repeated execution
#include <iostream>
#include <tt-metalium/host_api.hpp>
#include <tt-metalium/distributed.hpp>

using namespace tt::tt_metal;

int main(int argc, char** argv) {
    int num_runs = argc > 1 ? atoi(argv[1]) : 5;

    std::cout << "╔══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  Test: Program Cache for Repeated Execution             ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "Testing " << num_runs << " runs with program cache enabled..." << std::endl;

    auto mesh = distributed::MeshDevice::create_unit_mesh(0);
    auto& cq = mesh->mesh_command_queue();

    // ENABLE PROGRAM CACHE - This is the key!
    auto devices = mesh->get_devices();
    for (auto device : devices) {
        device->enable_program_cache();
        std::cout << "[Host] Enabled program cache on device" << std::endl;
    }

    std::cout << "\n[Host] Device ready, starting execution loop...\n" << std::endl;

    for (int i = 0; i < num_runs; i++) {
        std::cout << "[Host] Run " << (i+1) << "/" << num_runs << ": ";

        // Create program each time (should get cached after first run)
        Program program = CreateProgram();
        CreateKernel(program,
            "/home/ttuser/code/tt-zork1/kernels/noop_riscv.cpp",
            CoreCoord{0,0},
            DataMovementConfig{.processor = DataMovementProcessor::RISCV_0, .noc = NOC::RISCV_0_default});

        // Execute
        distributed::MeshWorkload workload;
        workload.add_program(distributed::MeshCoordinateRange(mesh->shape()), std::move(program));
        distributed::EnqueueMeshWorkload(cq, workload, false);
        distributed::Finish(cq);

        std::cout << "Complete!" << std::endl;
    }

    std::cout << "\n✅ SUCCESS: All " << num_runs << " runs completed!" << std::endl;
    mesh->close();
    return 0;
}
