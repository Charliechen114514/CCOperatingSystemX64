# ============================================================================
# CCOS Static Testing Framework
# ============================================================================
# This is an independent testing framework that validates kernel code using
# static analysis. It uses CTest for test execution and integrates with
# the main build to fail fast on any test errors.
# ============================================================================

cmake_minimum_required(VERSION 3.20)

enable_testing()

# ============================================================================
# Test Configuration Options
# ============================================================================
option(CCOS_TESTS_STRICT "Enable strict mode (warnings as errors)" ON)

# ============================================================================
# Compiler Flags - Match Kernel Standards (Hosted Variant)
# ============================================================================
# Key differences from kernel:
# - NO -ffreestanding (we need stdio for test output)
# - NO -nostdlib, -nostdinc, etc. (we link against libc)
# - KEEP: -Wall, -Wextra, -std=c23, -Werror (in strict mode)
# KEEP: Same warning levels to ensure kernel code compatibility

set(TEST_C_FLAGS "")

# Standard compliance - same as kernel
list(APPEND TEST_C_FLAGS "-std=c23")

# Warning levels - same strict standards as kernel
list(APPEND TEST_C_FLAGS "-Wall")
list(APPEND TEST_C_FLAGS "-Wextra")

# Additional useful warnings for testing
list(APPEND TEST_C_FLAGS "-Wpedantic")

# Treat warnings as errors in strict mode
if(CCOS_TESTS_STRICT)
    list(APPEND TEST_C_FLAGS "-Werror")
endif()

# Architecture flags - same as kernel
list(APPEND TEST_C_FLAGS "-m64")
list(APPEND TEST_C_FLAGS "-march=x86-64")

# Debug/Release configuration
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DCCOS_DEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Test Build type: ${CMAKE_BUILD_TYPE}")

# ============================================================================
# Test Include Directories
# ============================================================================
set(TEST_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/kernel/defines
    ${CMAKE_SOURCE_DIR}/kernel/assert
)

# ============================================================================
# Test: Type Definitions
# ============================================================================
add_executable(test_types
    test_defines.c
)

target_include_directories(test_types PRIVATE ${TEST_INCLUDE_DIRS})

# Override parent's freestanding flags for hosted test environment
target_compile_options(test_types PRIVATE
    ${TEST_C_FLAGS}
    -fno-builtin  # Still avoid builtins for stricter testing
)

# Remove freestanding-specific flags that were inherited from parent
# We need the standard library for test output
set_target_properties(test_types PROPERTIES
    COMPILE_FLAGS "-U_FREESTANDING_SOURCE"
)

# Register with CTest
add_test(NAME test_types COMMAND test_types)

# Set test properties
set_tests_properties(test_types PROPERTIES
    LABELS "static"
    TIMEOUT 10
)

# ----------------------------------------------------------------------------
# Test: Static Assertions
# ----------------------------------------------------------------------------
add_executable(test_static_assert
    test_static_assert.c
)

target_include_directories(test_static_assert PRIVATE ${TEST_INCLUDE_DIRS})
target_compile_options(test_static_assert PRIVATE ${TEST_C_FLAGS})

add_test(NAME test_static_assert COMMAND test_static_assert)

set_tests_properties(test_static_assert PROPERTIES
    LABELS "static"
    TIMEOUT 10
)

# ============================================================================
# Test Integration with Main Build
# ============================================================================
# Add a custom target that runs all tests and fails if any test fails
# This target can be added as a dependency to the main build targets
# to ensure tests pass before kernel is considered "built"

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND}
        --output-on-failure
        --label-regex "static"
        $<IF:$<CONFIG:Debug>,--extra-verbose,>
    DEPENDS test_types test_static_assert
    COMMENT "Running static tests..."
    VERBATIM
)

# Add check to the ALL build to enforce test-on-build
# Uncomment the following line to make tests run on every build
# add_custom_target(check_all ALL DEPENDS check)

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "==================================================")
message(STATUS "CCOS Static Test Suite Configured")
message(STATUS "==================================================")
message(STATUS "Available targets:")
message(STATUS "  make check        - Run all static tests")
message(STATUS "  ctest             - Run tests via CTest")
message(STATUS "  ctest -V          - Run tests with verbose output")
message(STATUS "  ctest -R <name>   - Run specific test by name")
message(STATUS "==================================================")
message(STATUS "")
