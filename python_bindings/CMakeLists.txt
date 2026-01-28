# SPDX-FileCopyrightText: © 2026 Tenstorrent Inc.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22...3.30)

project(zork_tt_bindings)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Python and PyBind11
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 REQUIRED)

# Find TT-Metalium (same as parent CMakeLists.txt)
find_package(TT-Metalium REQUIRED)

# Create Python module
pybind11_add_module(zork_tt zork_bindings.cpp)

# Link TT-Metalium
target_link_libraries(zork_tt PRIVATE TT::Metalium)

# Set C++20
target_compile_features(zork_tt PRIVATE cxx_std_20)

# Install to python_bindings directory for easy import
install(TARGETS zork_tt
        LIBRARY DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
