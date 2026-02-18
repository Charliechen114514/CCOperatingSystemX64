# ==============================================================================
# CCOS - Memory Size Configuration Generator
# ==============================================================================
# This script generates memory size configuration files for both bootloader
# (NASM .inc) and kernel (C .h) from a single source of truth.
#
# Features:
#   - Configurable memory size (default: 4GB, min: 16MB, max: 64GB)
#   - Validates against QEMU limits to prevent runtime crashes
#   - Generates consistent configuration for bootloader and kernel
# ==============================================================================

# ============================================================================
# Memory Size Configuration Options (Single Source of Truth)
# ============================================================================
# Modify these values to change the memory size. Both bootloader and kernel
# will automatically use the new values after re-running CMake.

# Default memory size: 4GB (4096 MB)
set(CCOS_MEMORY_SIZE_MB "4096" CACHE STRING
    "OS usable memory size in MB (min: 16, max: 65536, default: 4096)")

# Validation limits
set(CCOS_MIN_MEMORY_MB 16)
set(CCOS_MAX_MEMORY_MB 65536)  # 64GB practical limit for QEMU x86_64

# ============================================================================
# Validation
# ============================================================================

# Check if the value is a valid positive integer
if(NOT CCOS_MEMORY_SIZE_MB MATCHES "^[0-9]+$")
    message(FATAL_ERROR
        "Invalid memory size format: '${CCOS_MEMORY_SIZE_MB}'. "
        "Must be a positive integer (e.g., 4096 for 4GB).")
endif()

# Convert to integer for comparison
math(EXPR MEMORY_SIZE_VAL "${CCOS_MEMORY_SIZE_MB}")

# Check minimum
if(MEMORY_SIZE_VAL LESS ${CCOS_MIN_MEMORY_MB})
    message(FATAL_ERROR
        "Memory size too small: ${CCOS_MEMORY_SIZE_MB}MB "
        "(minimum: ${CCOS_MIN_MEMORY_MB}MB)")
endif()

# Check maximum
if(MEMORY_SIZE_VAL GREATER ${CCOS_MAX_MEMORY_MB})
    message(FATAL_ERROR
        "Memory size too large: ${CCOS_MEMORY_SIZE_MB}MB "
        "(maximum: ${CCOS_MAX_MEMORY_MB}MB)")
endif()

# ============================================================================
# Calculate derived values
# ============================================================================

# Memory size in various units for different use cases
math(EXPR CCOS_MEMORY_SIZE_BYTES "${MEMORY_SIZE_VAL} * 1024 * 1024")
math(EXPR CCOS_MEMORY_SIZE_KB "${MEMORY_SIZE_VAL} * 1024")

# Calculate pages (assuming 4KB pages)
math(EXPR CCOS_MEMORY_PAGES "(${CCOS_MEMORY_SIZE_BYTES} + 4095) / 4096")

# QEMU memory argument format (e.g., "4096M")
set(CCOS_QEMU_MEMORY_ARG "${MEMORY_SIZE_VAL}M")

# Format as human-readable string
if(MEMORY_SIZE_VAL GREATER_EQUAL 1024)
    math(EXPR CCOS_MEMORY_SIZE_GB "${MEMORY_SIZE_VAL} / 1024")
    math(EXPR CCOS_MEMORY_REM_MB "${MEMORY_SIZE_VAL} % 1024")
    if(CCOS_MEMORY_REM_MB EQUAL 0)
        set(CCOS_MEMORY_SIZE_HUMAN "${CCOS_MEMORY_SIZE_GB}GB")
    else()
        set(CCOS_MEMORY_SIZE_HUMAN "${CCOS_MEMORY_SIZE_GB}.${CCOS_MEMORY_REM_MB}GB")
    endif()
else()
    set(CCOS_MEMORY_SIZE_HUMAN "${MEMORY_SIZE_VAL}MB")
endif()

# ============================================================================
# Generate NASM Include File (for Bootloader)
# ============================================================================
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/MemSizeConfig.inc.in
    ${CMAKE_BINARY_DIR}/mem_size_constants.inc
    @ONLY
)

# ============================================================================
# Generate C Header File (for Kernel)
# ============================================================================
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/MemSizeConfig.h.in
    ${CMAKE_BINARY_DIR}/include/mem_size_config.h
    @ONLY
)

# ============================================================================
# Configuration Summary
# ============================================================================
message(STATUS "Memory Size Configuration:")
message(STATUS "  Memory Size:    ${CCOS_MEMORY_SIZE_HUMAN} (${CCOS_MEMORY_SIZE_MB}MB)")
message(STATUS "  Bytes:          ${CCOS_MEMORY_SIZE_BYTES}")
message(STATUS "  Pages (4KB):    ${CCOS_MEMORY_PAGES}")
message(STATUS "  QEMU Argument:  -m ${CCOS_QEMU_MEMORY_ARG}")
message(STATUS "  Generated files:")
message(STATUS "    - ${CMAKE_BINARY_DIR}/mem_size_constants.inc (bootloader)")
message(STATUS "    - ${CMAKE_BINARY_DIR}/include/mem_size_config.h (kernel)")
