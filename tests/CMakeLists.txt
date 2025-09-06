# Background Save Plugin - Tests CMakeLists.txt
# Copyright (c) 2024 钟芳道 (DJD)

cmake_minimum_required(VERSION 3.15)

# Test configuration
set(TEST_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/test_output")
set(GOLDEN_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/golden_data")
set(BENCHMARK_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/benchmark_output")

# Create test output directories
file(MAKE_DIRECTORY ${TEST_OUTPUT_DIR})
file(MAKE_DIRECTORY ${BENCHMARK_OUTPUT_DIR})

# Test library dependencies
find_package(Threads REQUIRED)

# Common test utilities library
add_library(test_utils STATIC
    test_utils.c
    test_utils.h
)

target_link_libraries(test_utils
    background_save
    ${CMAKE_THREAD_LIBS_INIT}
)

target_include_directories(test_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Golden Tests
add_executable(test_golden
    test_golden.c
)

target_link_libraries(test_golden
    test_utils
    background_save
    ${CMAKE_THREAD_LIBS_INIT}
)

target_compile_definitions(test_golden PRIVATE
    GOLDEN_TEST_DATA_DIR="${GOLDEN_DATA_DIR}"
    GOLDEN_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/golden_output"
)

# Benchmark Tests
add_executable(test_benchmark
    test_benchmark.c
)

target_link_libraries(test_benchmark
    test_utils
    background_save
    ${CMAKE_THREAD_LIBS_INIT}
)

target_compile_definitions(test_benchmark PRIVATE
    BENCHMARK_OUTPUT_DIR="${BENCHMARK_OUTPUT_DIR}"
)

# Add math library for benchmarks (required for statistical calculations)
if(UNIX)
    target_link_libraries(test_benchmark m)
    target_link_libraries(test_golden m)
endif()

# Main test suite
add_executable(test_suite
    test_main.c
)

target_link_libraries(test_suite
    test_utils
    background_save
    ${CMAKE_THREAD_LIBS_INIT}
)

# Link test modules
target_link_libraries(test_suite
    ${CMAKE_CURRENT_BINARY_DIR}/test_golden${CMAKE_EXECUTABLE_SUFFIX}
    ${CMAKE_CURRENT_BINARY_DIR}/test_benchmark${CMAKE_EXECUTABLE_SUFFIX}
)

# Test targets for CTest integration
enable_testing()

# Golden Tests - Individual test cases
add_test(NAME golden_empty_database
         COMMAND test_golden --test=empty_database
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME golden_simple_strings
         COMMAND test_golden --test=simple_strings
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME golden_integers
         COMMAND test_golden --test=integers
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME golden_binary_data
         COMMAND test_golden --test=binary_data
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME golden_large_dataset
         COMMAND test_golden --test=large_dataset
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Golden Tests - All tests
add_test(NAME golden_all
         COMMAND test_golden
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Benchmark Tests
add_test(NAME benchmark_small
         COMMAND test_benchmark --test=small
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME benchmark_medium
         COMMAND test_benchmark --test=medium
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME benchmark_large
         COMMAND test_benchmark --test=large
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME benchmark_compression
         COMMAND test_benchmark --test=compression
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Benchmark Tests - All benchmarks
add_test(NAME benchmark_all
         COMMAND test_benchmark
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Full test suite
add_test(NAME test_suite_all
         COMMAND test_suite --all
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME test_suite_golden_only
         COMMAND test_suite --golden
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME test_suite_benchmark_only
         COMMAND test_suite --benchmark
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Test properties
set_tests_properties(golden_empty_database PROPERTIES
    LABELS "golden;unit"
    TIMEOUT 30
)

set_tests_properties(golden_simple_strings PROPERTIES
    LABELS "golden;unit"
    TIMEOUT 30
)

set_tests_properties(golden_integers PROPERTIES
    LABELS "golden;unit"
    TIMEOUT 30
)

set_tests_properties(golden_binary_data PROPERTIES
    LABELS "golden;unit"
    TIMEOUT 30
)

set_tests_properties(golden_large_dataset PROPERTIES
    LABELS "golden;stress"
    TIMEOUT 120
)

set_tests_properties(golden_all PROPERTIES
    LABELS "golden;integration"
    TIMEOUT 300
)

set_tests_properties(benchmark_small PROPERTIES
    LABELS "benchmark;performance"
    TIMEOUT 60
)

set_tests_properties(benchmark_medium PROPERTIES
    LABELS "benchmark;performance"
    TIMEOUT 120
)

set_tests_properties(benchmark_large PROPERTIES
    LABELS "benchmark;performance;stress"
    TIMEOUT 300
)

set_tests_properties(benchmark_compression PROPERTIES
    LABELS "benchmark;performance"
    TIMEOUT 180
)

set_tests_properties(benchmark_all PROPERTIES
    LABELS "benchmark;performance;integration"
    TIMEOUT 600
)

set_tests_properties(test_suite_all PROPERTIES
    LABELS "integration;full"
    TIMEOUT 900
)

set_tests_properties(test_suite_golden_only PROPERTIES
    LABELS "golden;integration"
    TIMEOUT 300
)

set_tests_properties(test_suite_benchmark_only PROPERTIES
    LABELS "benchmark;integration"
    TIMEOUT 600
)

# Custom targets for convenient test running

# Quick tests (golden tests only)
add_custom_target(test_quick
    COMMAND ${CMAKE_CTEST_COMMAND} -L "golden" --output-on-failure
    DEPENDS test_golden
    COMMENT "Running quick golden tests"
)

# Performance tests (benchmarks only)
add_custom_target(test_perf
    COMMAND ${CMAKE_CTEST_COMMAND} -L "benchmark" --output-on-failure
    DEPENDS test_benchmark
    COMMENT "Running performance benchmarks"
)

# Stress tests (large datasets and long-running tests)
add_custom_target(test_stress
    COMMAND ${CMAKE_CTEST_COMMAND} -L "stress" --output-on-failure
    DEPENDS test_golden test_benchmark
    COMMENT "Running stress tests"
)

# Full test suite
add_custom_target(test_all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_suite
    COMMENT "Running full test suite"
)

# Test with verbose output
add_custom_target(test_verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose --output-on-failure
    DEPENDS test_suite
    COMMENT "Running tests with verbose output"
)

# Memory tests (if valgrind is available)
find_program(VALGRIND_PROGRAM valgrind)
if(VALGRIND_PROGRAM AND NOT WIN32)
    add_custom_target(test_memory
        COMMAND ${CMAKE_CTEST_COMMAND} -T memcheck
        DEPENDS test_golden
        COMMENT "Running memory tests with valgrind"
    )

    # Configure CTest to use valgrind
    set(MEMORYCHECK_COMMAND ${VALGRIND_PROGRAM})
    set(MEMORYCHECK_COMMAND_OPTIONS "--leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1")
endif()

# Test coverage (if gcov is available)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCC)
    find_program(GCOV_PROGRAM gcov)
    find_program(LCOV_PROGRAM lcov)
    find_program(GENHTML_PROGRAM genhtml)

    if(GCOV_PROGRAM AND LCOV_PROGRAM AND GENHTML_PROGRAM)
        add_custom_target(test_coverage
            # Reset coverage counters
            COMMAND ${LCOV_PROGRAM} --directory . --zerocounters

            # Run tests
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure

            # Capture coverage data
            COMMAND ${LCOV_PROGRAM} --directory . --capture --output-file coverage.info

            # Remove system/external headers from coverage
            COMMAND ${LCOV_PROGRAM} --remove coverage.info '/usr/*' --output-file coverage_filtered.info

            # Generate HTML report
            COMMAND ${GENHTML_PROGRAM} coverage_filtered.info --output-directory coverage_html

            DEPENDS test_golden test_benchmark
            COMMENT "Generating test coverage report"
        )

        message(STATUS "Test coverage target enabled. Run 'make test_coverage' to generate coverage report.")
    endif()
endif()

# Test data generation
add_custom_target(generate_test_data
    COMMAND ${CMAKE_COMMAND} -E echo "Generating test data..."
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_golden_data.sh
    COMMENT "Generating golden test data"
)

# Clean test outputs
add_custom_target(clean_tests
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${TEST_OUTPUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${BENCHMARK_OUTPUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/golden_output
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_OUTPUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BENCHMARK_OUTPUT_DIR}
    COMMENT "Cleaning test output directories"
)

# Help target
add_custom_target(test_help
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Available test targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_quick      - Run quick golden tests only"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_perf       - Run performance benchmarks only"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_stress     - Run stress tests (large datasets)"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_all        - Run all tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_verbose    - Run tests with verbose output"
    COMMAND ${CMAKE_COMMAND} -E echo "  clean_tests     - Clean test output directories"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "CTest commands:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest                           - Run all tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest -L golden                 - Run golden tests only"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest -L benchmark              - Run benchmarks only"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest -L performance            - Run performance tests"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest --verbose                 - Run with verbose output"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest --output-on-failure       - Show output on failure"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMENT "Display test help"
)

# Install test binaries (optional)
if(RBS_INSTALL_TESTS)
    install(TARGETS test_golden test_benchmark test_suite
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/tests
        COMPONENT tests
    )

    install(DIRECTORY ${GOLDEN_DATA_DIR}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/background-save-plugin/tests
        COMPONENT tests
    )
endif()

# Print test configuration summary
message(STATUS "")
message(STATUS "Test Configuration:")
message(STATUS "  Golden tests:     Enabled")
message(STATUS "  Benchmark tests:  Enabled")
message(STATUS "  Test output dir:  ${TEST_OUTPUT_DIR}")
message(STATUS "  Golden data dir:  ${GOLDEN_DATA_DIR}")
message(STATUS "  Benchmark dir:    ${BENCHMARK_OUTPUT_DIR}")
if(VALGRIND_PROGRAM)
    message(STATUS "  Memory tests:     Enabled (valgrind)")
endif()
if(GCOV_PROGRAM AND LCOV_PROGRAM AND GENHTML_PROGRAM)
    message(STATUS "  Coverage tests:   Enabled (gcov/lcov)")
endif()
message(STATUS "")
message(STATUS "Run 'make test_help' for available test targets.")
message(STATUS "")