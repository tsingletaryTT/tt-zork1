cmake_minimum_required(VERSION 3.22...3.30)
project(zork_on_blackhole)

# Find TT-Metalium package
find_package(TT-Metalium REQUIRED)

# Host program
add_executable(zork_on_blackhole)
target_sources(zork_on_blackhole PRIVATE zork_on_blackhole.cpp)
target_link_libraries(zork_on_blackhole PUBLIC TT::Metalium)

# Minimal device init test
add_executable(test_device_init)
target_sources(test_device_init PRIVATE test_device_init.cpp)
target_link_libraries(test_device_init PUBLIC TT::Metalium)

# TODO: Add kernel compilation target later
