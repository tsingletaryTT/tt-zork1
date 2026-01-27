#include <iostream>
#include <tt-metalium/host_api.hpp>
#include <tt-metalium/distributed.hpp>

using namespace tt::tt_metal;

int main() {
    std::cout << "Testing multiple program executions on same device..." << std::endl;
    
    auto mesh = distributed::MeshDevice::create_unit_mesh(0);
    std::cout << "Device created" << std::endl;
    
    for (int i = 0; i < 3; i++) {
        std::cout << "Run " << (i+1) << ": Creating and executing program..." << std::endl;
        
        // Create fresh program each time (like we do now)
        Program program = CreateProgram();
        
        // Simple kernel that does nothing
        CreateKernel(program, 
            "/home/ttuser/tt-metal/tt_metal/programming_examples/hello_world_datamovement_kernel/kernels/hello_world.cpp",
            CoreCoord{0,0},
            DataMovementConfig{.processor = DataMovementProcessor::RISCV_0, .noc = NOC::RISCV_0_default});
        
        // Execute
        distributed::MeshWorkload workload;
        workload.add_program(distributed::MeshCoordinateRange(mesh->shape()), std::move(program));
        distributed::EnqueueMeshWorkload(mesh->mesh_command_queue(), workload, false);
        distributed::Finish(mesh->mesh_command_queue());
        
        std::cout << "Run " << (i+1) << " complete!" << std::endl;
    }
    
    mesh->close();
    std::cout << "All runs completed successfully!" << std::endl;
    return 0;
}
