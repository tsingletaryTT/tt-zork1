cmake_minimum_required(VERSION 3.22...3.30)
project(zork_on_blackhole)

# Find TT-Metalium package
find_package(TT-Metalium REQUIRED)

# Host program - single-shot execution
add_executable(zork_on_blackhole)
target_sources(zork_on_blackhole PRIVATE zork_on_blackhole.cpp)
target_link_libraries(zork_on_blackhole PUBLIC TT::Metalium)

# Host program - batched execution with state persistence
add_executable(zork_batched)
target_sources(zork_batched PRIVATE zork_batched.cpp)
target_link_libraries(zork_batched PUBLIC TT::Metalium)

# Minimal device init test
add_executable(test_device_init)
target_sources(test_device_init PRIVATE test_device_init.cpp)
target_link_libraries(test_device_init PUBLIC TT::Metalium)

# Simple kernel test (device init + kernel creation)
add_executable(test_simple_kernel)
target_sources(test_simple_kernel PRIVATE test_simple_kernel.cpp)
target_link_libraries(test_simple_kernel PUBLIC TT::Metalium)

# Test with full program structure (device init only)
add_executable(test_full_structure)
target_sources(test_full_structure PRIVATE test_full_structure.cpp)
target_link_libraries(test_full_structure PUBLIC TT::Metalium)

# Test with buffer allocation
add_executable(test_buffers)
target_sources(test_buffers PRIVATE test_buffers.cpp)
target_link_libraries(test_buffers PUBLIC TT::Metalium)

# Test with minimal hello world kernel
add_executable(test_hello_kernel)
target_sources(test_hello_kernel PRIVATE test_hello_kernel.cpp)
target_link_libraries(test_hello_kernel PUBLIC TT::Metalium)

# Test simple state persistence
add_executable(test_state_simple)
target_sources(test_state_simple PRIVATE test_state_simple.cpp)
target_link_libraries(test_state_simple PUBLIC TT::Metalium)

# TODO: Add kernel compilation target later
add_executable(zork_streaming)
target_sources(zork_streaming PRIVATE ../zork_on_blackhole_streaming.cpp)
target_link_libraries(zork_streaming PUBLIC TT::Metalium)

add_executable(test_multiple_runs)
target_sources(test_multiple_runs PRIVATE ../test_multiple_runs.cpp)
target_link_libraries(test_multiple_runs PUBLIC TT::Metalium)

add_executable(test_cached_execution)
target_sources(test_cached_execution PRIVATE ../test_cached_execution.cpp)
target_link_libraries(test_cached_execution PUBLIC TT::Metalium)
