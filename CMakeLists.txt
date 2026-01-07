cmake_minimum_required(VERSION 3.16)
project(SpectralGate VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/hal
    ${CMAKE_SOURCE_DIR}/data
)

# Core library (hardware-independent)
add_library(spectral_core STATIC
    src/core/decision.cpp
    src/core/inference.cpp
    src/core/spectral.cpp
)

# HAL Mock library (PC simulation)
add_library(hal_mock STATIC
    src/hal/hal_mock.cpp
)

# Main executable
add_executable(spectral_gate
    src/main.cpp
)

target_link_libraries(spectral_gate
    spectral_core
    hal_mock
)

# Tests (optional, placeholder)
enable_testing()
add_subdirectory(tests)
