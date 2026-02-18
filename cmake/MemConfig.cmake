# ==============================================================================
# CCOS - Memory Configuration Generator
# ==============================================================================
# This script generates consistent memory configuration files for both
# bootloader (NASM .inc) and kernel (C .h) from a single source of truth.
# ==============================================================================

# ============================================================================
# Memory Configuration Options (Single Source of Truth)
# ============================================================================
# Modify these values to change the memory layout. Both bootloader and kernel
# will automatically use the new values after re-running CMake.

# E820 Storage Address - Where bootloader stores memory map for kernel
# Must not overlap with:
#   - Bootloader code (0x7C00 - 0x8C00, ~4KB)
#   - Page table areas (0x9000-0xBFFF)
# Default: 0x6000 (24KB, safely before bootloader)
set(E820_STORAGE_ADDR_DEFAULT "0x6000" CACHE STRING
    "E820 memory map storage address (hex format, e.g., 0x6000)")

# Maximum number of E820 memory entries
# Default: 128 entries (128 * 24 bytes = ~3KB, fits in 4KB space)
set(E820_MAX_ENTRIES_DEFAULT "128" CACHE STRING
    "Maximum number of E820 memory entries")

# Validate E820_STORAGE_ADDR format
if(NOT E820_STORAGE_ADDR_DEFAULT MATCHES "^0x[0-9A-Fa-f]+$")
    message(FATAL_ERROR "E820_STORAGE_ADDR must be in hex format (e.g., 0x6000)")
endif()

# ============================================================================
# Convert hex address to decimal and string formats for different targets
# ============================================================================
# Strip "0x" prefix for numeric processing
string(REGEX REPLACE "^0x" "" E820_ADDR_HEX_ONLY "${E820_STORAGE_ADDR_DEFAULT}")

# Calculate decimal value (for potential future use)
math(EXPR E820_ADDR_DECIMAL "0x${E820_ADDR_HEX_ONLY}")

# Set variables for @ substitution in template files
set(E820_STORAGE_ADDR_HEX "${E820_ADDR_HEX_ONLY}")
set(E820_MAX_ENTRIES "${E820_MAX_ENTRIES_DEFAULT}")

# ============================================================================
# Generate NASM Include File (for Bootloader)
# ============================================================================
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/MemConfig.inc.in
    ${CMAKE_BINARY_DIR}/mem_detect_constants.inc
    @ONLY
)

# ============================================================================
# Generate C Header File (for Kernel)
# ============================================================================
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/MemConfig.h.in
    ${CMAKE_BINARY_DIR}/include/mem_config.h
    @ONLY
)

# ============================================================================
# Validate Configuration with Python Script
# ============================================================================
# Run the validation script to check for memory conflicts
execute_process(
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/validate_e820_config.py
        ${E820_STORAGE_ADDR_DEFAULT}
        ${E820_MAX_ENTRIES_DEFAULT}
        -q
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE VALIDATION_RESULT
    OUTPUT_VARIABLE VALIDATION_OUTPUT
    ERROR_VARIABLE VALIDATION_ERROR
)

# If validation failed, show output and error
if(NOT VALIDATION_RESULT EQUAL 0)
    message(WARNING "E820 configuration validation failed:")
    message(WARNING "${VALIDATION_OUTPUT}")
    if(VALIDATION_ERROR)
        message(WARNING "${VALIDATION_ERROR}")
    endif()
    message(WARNING "To see safe alternatives, run:")
    message(WARNING "  cd ${CMAKE_SOURCE_DIR}")
    message(WARNING "  python3 scripts/validate_e820_config.py ${E820_STORAGE_ADDR_DEFAULT} ${E820_MAX_ENTRIES_DEFAULT}")
endif()

# ============================================================================
# Configuration Summary
# ============================================================================
message(STATUS "Memory Configuration:")
message(STATUS "  E820 Storage Address: ${E820_STORAGE_ADDR_DEFAULT}")
message(STATUS "  E820 Max Entries:     ${E820_MAX_ENTRIES_DEFAULT}")
message(STATUS "  Generated files:")
message(STATUS "    - ${CMAKE_BINARY_DIR}/mem_detect_constants.inc (bootloader)")
message(STATUS "    - ${CMAKE_BINARY_DIR}/include/mem_config.h (kernel)")
if(NOT VALIDATION_RESULT EQUAL 0)
    message(STATUS "  WARNING: Configuration validation failed - see warnings above")
endif()
