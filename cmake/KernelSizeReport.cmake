# ==============================================================================
# CCOS - Kernel Size and Memory Report Generator
# ==============================================================================
# This script generates a comprehensive report of kernel size and memory config
# Usage: Called as POST_BUILD step or standalone with -DKERNEL_BIN=file.bin
# ==============================================================================

# Get kernel binary path from argument or use default
if(NOT DEFINED KERNEL_BIN)
    set(KERNEL_BIN "${KERNEL_BIN}" CACHE FILEPATH "Path to kernel.bin")
endif()

if(NOT DEFINED CCOS_MEMORY_SIZE_MB)
    set(CCOS_MEMORY_SIZE_MB "${CCOS_MEMORY_SIZE_MB}" CACHE STRING "Memory size in MB")
endif()

if(NOT DEFINED CCOS_MEMORY_SIZE_HUMAN)
    set(CCOS_MEMORY_SIZE_HUMAN "${CCOS_MEMORY_SIZE_HUMAN}" CACHE STRING "Human-readable memory size")
endif()

# Validate kernel binary exists
if(NOT EXISTS "${KERNEL_BIN}")
    message(FATAL_ERROR "Kernel binary not found: ${KERNEL_BIN}")
endif()

# ============================================================================
# Get kernel size
# ============================================================================
file(SIZE "${KERNEL_BIN}" KERNEL_SIZE_BYTES)

# Calculate various size representations
math(EXPR KERNEL_SIZE_KB "(${KERNEL_SIZE_BYTES} + 1023) / 1024")
math(EXPR KERNEL_SIZE_MB "(${KERNEL_SIZE_BYTES} + 1048575) / 1048576")

# Calculate sectors (512-byte sectors for disk layout)
math(EXPR KERNEL_SIZE_SECTORS "(${KERNEL_SIZE_BYTES} + 511) / 512")

# Format kernel size as human-readable
# Use integer division (no rounding) to get actual MB count
math(EXPR KERNEL_SIZE_MB_INT "${KERNEL_SIZE_BYTES} / 1048576")
if(KERNEL_SIZE_MB_INT GREATER 0)
    math(EXPR KERNEL_REM_KB "${KERNEL_SIZE_KB} - (${KERNEL_SIZE_MB_INT} * 1024)")
    if(KERNEL_REM_KB EQUAL 0)
        set(KERNEL_SIZE_HUMAN "${KERNEL_SIZE_MB_INT}MB")
    else()
        set(KERNEL_SIZE_HUMAN "${KERNEL_SIZE_MB_INT}.${KERNEL_REM_KB}MB")
    endif()
else()
    set(KERNEL_SIZE_HUMAN "${KERNEL_SIZE_KB}KB")
endif()

# ============================================================================
# Calculate memory utilization percentage
# ============================================================================
math(EXPR MEMORY_BYTES "${CCOS_MEMORY_SIZE_MB} * 1024 * 1024")
math(EXPR KERNEL_PERCENT "(${KERNEL_SIZE_BYTES} * 100) / ${MEMORY_BYTES}")

# ============================================================================
# Print Report
# ============================================================================
message(STATUS "")
message(STATUS "================================================================================")
message(STATUS "  KERNEL BUILD REPORT")
message(STATUS "================================================================================")
message(STATUS "Kernel Binary:")
message(STATUS "  File:     ${KERNEL_BIN}")
message(STATUS "  Size:     ${KERNEL_SIZE_BYTES} bytes")
message(STATUS "            ${KERNEL_SIZE_KB} KB")
message(STATUS "            ${KERNEL_SIZE_HUMAN}")
message(STATUS "            ${KERNEL_SIZE_SECTORS} sectors (512-byte)")
message(STATUS "")
message(STATUS "Memory Configuration:")
message(STATUS "  Total:    ${CCOS_MEMORY_SIZE_HUMAN} (${CCOS_MEMORY_SIZE_MB} MB = ${MEMORY_BYTES} bytes)")
message(STATUS "  Util:     ${KERNEL_SIZE_BYTES} bytes / ${MEMORY_BYTES} bytes = ${KERNEL_PERCENT}%")
message(STATUS "")
message(STATUS "Summary: Kernel uses ${KERNEL_PERCENT}% of configured memory")
message(STATUS "================================================================================")
message(STATUS "")

# Optional: Write report to file for build artifacts
set(REPORT_FILE "${CMAKE_BINARY_DIR}/kernel_size_report.txt")
file(WRITE "${REPORT_FILE}" "================================================================================\n")
file(APPEND "${REPORT_FILE}" "  CCOS KERNEL BUILD REPORT\n")
file(APPEND "${REPORT_FILE}" "================================================================================\n")
file(APPEND "${REPORT_FILE}" "Kernel Binary:\n")
file(APPEND "${REPORT_FILE}" "  File:     ${KERNEL_BIN}\n")
file(APPEND "${REPORT_FILE}" "  Size:     ${KERNEL_SIZE_BYTES} bytes (${KERNEL_SIZE_HUMAN})\n")
file(APPEND "${REPORT_FILE}" "            ${KERNEL_SIZE_SECTORS} sectors (512-byte)\n")
file(APPEND "${REPORT_FILE}" "\n")
file(APPEND "${REPORT_FILE}" "Memory Configuration:\n")
file(APPEND "${REPORT_FILE}" "  Total:    ${CCOS_MEMORY_SIZE_HUMAN} (${CCOS_MEMORY_SIZE_MB} MB)\n")
file(APPEND "${REPORT_FILE}" "  Utilization: ${KERNEL_PERCENT}%\n")
file(APPEND "${REPORT_FILE}" "================================================================================\n")

message(STATUS "Report saved to: ${REPORT_FILE}")
