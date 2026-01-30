// Zork REPL - Native Unix-style Read-Eval-Print Loop
// Keeps device warm and program compiled for fast command execution
//
// Architecture inspired by vLLM's worker pattern:
// - Initialize device ONCE at startup
// - Compile kernel ONCE (not per-command)
// - Allocate buffers ONCE (reuse across commands)
// - Only input buffer changes between commands
//
// Player experience goal: 100-300ms per command

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device.hpp>
#include <tt-metalium/program.hpp>
#include <tt-metalium/distributed.hpp>
#include <tt-metalium/system_mesh.hpp>

using namespace tt;
using namespace tt::tt_metal;
using namespace std::chrono;

// Constants
constexpr uint32_t MAX_GAME_SIZE = 128 * 1024;    // 128KB for game file
constexpr uint32_t MAX_OUTPUT_SIZE = 16 * 1024;   // 16KB output buffer
constexpr uint32_t MAX_INPUT_SIZE = 1024;         // 1KB input buffer
constexpr uint32_t MAX_STATE_SIZE = 16 * 1024;    // 16KB state buffer
constexpr CoreCoord ZORK_CORE = {0, 0};

class ZorkREPL {
private:
    // Device resources (initialized ONCE)
    std::shared_ptr<distributed::MeshDevice> parent_mesh;
    std::shared_ptr<distributed::MeshDevice> mesh_device;
    distributed::MeshCommandQueue* cq;

    // Buffers (allocated ONCE)
    std::shared_ptr<distributed::MeshBuffer> game_buffer;
    std::shared_ptr<distributed::MeshBuffer> output_buffer;
    std::shared_ptr<distributed::MeshBuffer> input_buffer;
    std::shared_ptr<distributed::MeshBuffer> state_buffer;

    // Kernel defines (set once, reused for program compilation)
    std::map<std::string, std::string> kernel_defines;

    // Game data
    std::vector<uint8_t> game_data;
    std::vector<uint8_t> state_data;

    // Statistics
    int commands_executed;

public:
    ZorkREPL() : commands_executed(0) {}

    // Initialize device and allocate buffers (called once at startup)
    void init(const std::string& game_file) {
        std::cout << "╔══════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║  ZORK I on Tenstorrent Blackhole RISC-V - REPL Mode     ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════════════════╝" << std::endl;
        std::cout << std::endl;

        auto start_time = high_resolution_clock::now();

        // Load game file
        std::cout << "[Init] Loading game file... " << std::flush;
        game_data = load_game_file(game_file);
        std::cout << "✓ (" << game_data.size() << " bytes)" << std::endl;

        // Initialize device using parent_mesh → submesh pattern
        std::cout << "[Init] Initializing Blackhole device... " << std::flush;
        const auto system_mesh_shape = distributed::SystemMesh::instance().shape();
        parent_mesh = distributed::MeshDevice::create(
            distributed::MeshDeviceConfig(system_mesh_shape)
        );
        mesh_device = parent_mesh->create_submesh(distributed::MeshShape(1, 1));
        std::cout << "✓" << std::endl;

        // Enable program cache
        std::cout << "[Init] Enabling program cache... " << std::flush;
        auto devices = mesh_device->get_devices();
        for (auto device : devices) {
            device->enable_program_cache();
        }
        std::cout << "✓" << std::endl;

        cq = &mesh_device->mesh_command_queue();

        // Allocate buffers ONCE
        std::cout << "[Init] Allocating DRAM buffers... " << std::flush;

        distributed::DeviceLocalBufferConfig game_dram_config{
            .page_size = MAX_GAME_SIZE,
            .buffer_type = BufferType::DRAM
        };
        distributed::DeviceLocalBufferConfig output_dram_config{
            .page_size = MAX_OUTPUT_SIZE,
            .buffer_type = BufferType::DRAM
        };
        distributed::DeviceLocalBufferConfig input_dram_config{
            .page_size = MAX_INPUT_SIZE,
            .buffer_type = BufferType::DRAM
        };
        distributed::DeviceLocalBufferConfig state_dram_config{
            .page_size = MAX_STATE_SIZE,
            .buffer_type = BufferType::DRAM
        };

        game_buffer = distributed::MeshBuffer::create(
            distributed::ReplicatedBufferConfig{.size = MAX_GAME_SIZE},
            game_dram_config,
            mesh_device.get()
        );

        output_buffer = distributed::MeshBuffer::create(
            distributed::ReplicatedBufferConfig{.size = MAX_OUTPUT_SIZE},
            output_dram_config,
            mesh_device.get()
        );

        input_buffer = distributed::MeshBuffer::create(
            distributed::ReplicatedBufferConfig{.size = MAX_INPUT_SIZE},
            input_dram_config,
            mesh_device.get()
        );

        state_buffer = distributed::MeshBuffer::create(
            distributed::ReplicatedBufferConfig{.size = MAX_STATE_SIZE},
            state_dram_config,
            mesh_device.get()
        );

        std::cout << "✓" << std::endl;

        // Upload game data ONCE
        std::cout << "[Init] Uploading game data to DRAM... " << std::flush;
        std::vector<uint8_t> game_data_padded(MAX_GAME_SIZE, 0);
        std::copy(game_data.begin(), game_data.end(), game_data_padded.begin());
        distributed::EnqueueWriteMeshBuffer(*cq, game_buffer, game_data_padded, /*blocking=*/true);
        std::cout << "✓" << std::endl;

        // Initialize state
        std::cout << "[Init] Initializing Z-machine state... " << std::flush;
        state_data.resize(MAX_STATE_SIZE, 0);
        distributed::EnqueueWriteMeshBuffer(*cq, state_buffer, state_data, /*blocking=*/true);
        std::cout << "✓" << std::endl;

        // Set up kernel defines (buffer addresses)
        std::cout << "[Init] Setting up kernel configuration... " << std::flush;
        setup_kernel_defines();
        std::cout << "✓" << std::endl;

        auto end_time = high_resolution_clock::now();
        auto init_ms = duration_cast<milliseconds>(end_time - start_time).count();

        std::cout << std::endl;
        std::cout << "═══════════════════════════════════════════════════════════" << std::endl;
        std::cout << "Initialization complete in " << init_ms << "ms" << std::endl;
        std::cout << "Device is warm, kernel is compiled, ready for commands!" << std::endl;
        std::cout << "═══════════════════════════════════════════════════════════" << std::endl;
        std::cout << std::endl;

        // Execute first batch to initialize game state
        std::cout << "[Init] Initializing game... " << std::flush;
        execute_batch("");
        std::cout << "✓" << std::endl;
        std::cout << std::endl;
    }

    // Set up kernel defines (buffer addresses - done once)
    void setup_kernel_defines() {
        char addr_buf[32];

        snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)game_buffer->address());
        kernel_defines["GAME_DRAM_ADDR"] = addr_buf;

        snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)output_buffer->address());
        kernel_defines["OUTPUT_DRAM_ADDR"] = addr_buf;

        snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)input_buffer->address());
        kernel_defines["INPUT_DRAM_ADDR"] = addr_buf;

        snprintf(addr_buf, sizeof(addr_buf), "0x%lx", (unsigned long)state_buffer->address());
        kernel_defines["STATE_DRAM_ADDR"] = addr_buf;
    }

    // Create program with kernel (relies on program cache for speed)
    Program create_program() {
        Program program = CreateProgram();

        CreateKernel(
            program,
            "/home/ttuser/code/tt-zork1/kernels/zork_interpreter_opt.cpp",
            ZORK_CORE,
            DataMovementConfig{
                .processor = DataMovementProcessor::RISCV_0,
                .noc = NOC::RISCV_0_default,
                .defines = kernel_defines
            }
        );

        return program;
    }

    // Execute a batch (reuses compiled program!)
    void execute_batch(const std::string& input) {
        // Update input buffer
        std::vector<uint8_t> input_data(MAX_INPUT_SIZE, 0);
        if (!input.empty()) {
            std::strncpy(reinterpret_cast<char*>(input_data.data()),
                        input.c_str(), MAX_INPUT_SIZE - 1);
        }
        distributed::EnqueueWriteMeshBuffer(*cq, input_buffer, input_data, /*blocking=*/true);

        // Create program (program cache makes this fast after first time!)
        Program program = create_program();

        // Execute kernel
        distributed::MeshWorkload workload;
        distributed::MeshCoordinateRange device_range(mesh_device->shape());
        workload.add_program(device_range, std::move(program));

        distributed::EnqueueMeshWorkload(*cq, workload, /*blocking=*/false);
        distributed::Finish(*cq);

        // Read output
        std::vector<char> output_data(MAX_OUTPUT_SIZE);
        distributed::EnqueueReadMeshBuffer(*cq, output_data, output_buffer, /*blocking=*/true);

        // Read state (for persistence)
        distributed::EnqueueReadMeshBuffer(*cq, state_data, state_buffer, /*blocking=*/true);

        // Display output
        std::string output(output_data.data());
        if (!output.empty()) {
            std::cout << output << std::endl;
        }

        commands_executed++;
    }

    // Main REPL loop
    void repl() {
        std::cout << "Type commands or 'quit' to exit." << std::endl;
        std::cout << std::endl;

        std::string line;
        while (true) {
            std::cout << "> " << std::flush;

            if (!std::getline(std::cin, line)) {
                break;  // EOF or error
            }

            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (line.empty()) {
                continue;
            }

            // Check for quit command
            if (line == "quit" || line == "exit" || line == "q") {
                std::cout << std::endl;
                std::cout << "Thanks for playing!" << std::endl;
                break;
            }

            // Execute command and measure time
            auto start = high_resolution_clock::now();
            execute_batch(line);
            auto end = high_resolution_clock::now();

            auto ms = duration_cast<milliseconds>(end - start).count();
            std::cout << "[" << ms << "ms]" << std::endl;
            std::cout << std::endl;
        }

        // Statistics
        std::cout << std::endl;
        std::cout << "═══════════════════════════════════════════════════════════" << std::endl;
        std::cout << "Session Statistics:" << std::endl;
        std::cout << "  Commands executed: " << commands_executed << std::endl;
        std::cout << "═══════════════════════════════════════════════════════════" << std::endl;
    }

    // Cleanup
    void cleanup() {
        std::cout << std::endl;
        std::cout << "[Cleanup] Closing devices... " << std::flush;
        mesh_device->close();
        parent_mesh->close();
        std::cout << "✓" << std::endl;
    }

private:
    std::vector<uint8_t> load_game_file(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) {
            throw std::runtime_error("Cannot open game file: " + filename);
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            throw std::runtime_error("Cannot read game file: " + filename);
        }

        return buffer;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <game_file.z3>" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Example:" << std::endl;
        std::cerr << "  " << argv[0] << " game/zork1.z3" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Interactive mode:" << std::endl;
        std::cerr << "  Commands are read from stdin" << std::endl;
        std::cerr << "  Type 'quit' to exit" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Batch mode:" << std::endl;
        std::cerr << "  echo 'open mailbox' | " << argv[0] << " game/zork1.z3" << std::endl;
        std::cerr << "  cat commands.txt | " << argv[0] << " game/zork1.z3" << std::endl;
        return 1;
    }

    try {
        ZorkREPL repl;
        repl.init(argv[1]);
        repl.repl();
        repl.cleanup();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
