# Tests CMakeLists.txt
# Placeholder for unit tests

# Option to enable/disable tests
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    # Test executable placeholder
    add_executable(spectral_gate_tests
        test_main.cpp
    )

    target_link_libraries(spectral_gate_tests
        spectral_core
        hal_mock
    )

    # Register with CTest
    add_test(NAME SpectralGateTests COMMAND spectral_gate_tests)
endif()
