# ==============================================================================
# CCOS Boot Image Assembly Script
# ==============================================================================
# Assembles boot.img from bootloader.bin and kernel.bin using dd
# Dynamically calculates sector counts and offsets
# ==============================================================================

set(BOOTLOADER_BIN "${BOOTLOADER_BIN}" CACHE FILEPATH "Path to bootloader.bin")
set(KERNEL_BIN "${KERNEL_BIN}" CACHE FILEPATH "Path to kernel.bin")
set(BOOT_IMG "${BOOT_IMG}" CACHE FILEPATH "Output boot.img path")

# Get file sizes
file(SIZE "${BOOTLOADER_BIN}" BOOTLOADER_BYTES)
file(SIZE "${KERNEL_BIN}" KERNEL_BYTES)

# Calculate sectors
math(EXPR BOOTLOADER_SECTORS "(${BOOTLOADER_BYTES} + 511) / 512")
math(EXPR KERNEL_SECTORS "(${KERNEL_BYTES} + 511) / 512")

message(STATUS "Bootloader: ${BOOTLOADER_BYTES} bytes = ${BOOTLOADER_SECTORS} sectors")
message(STATUS "Kernel: ${KERNEL_BYTES} bytes = ${KERNEL_SECTORS} sectors")

# Execute dd commands
# Step 1: Write bootloader (creates the image)
execute_process(
    COMMAND dd
        if=${BOOTLOADER_BIN}
        of=${BOOT_IMG}
        bs=512
        count=${BOOTLOADER_SECTORS}
        conv=notrunc
    OUTPUT_QUIET
    ERROR_QUIET
    RESULT_VARIABLE DD_RESULT
)

if(NOT DD_RESULT EQUAL 0)
    message(FATAL_ERROR "dd command failed for bootloader (result: ${DD_RESULT})")
endif()

# Step 2: Append kernel at offset after bootloader
execute_process(
    COMMAND dd
        if=${KERNEL_BIN}
        of=${BOOT_IMG}
        bs=512
        seek=${BOOTLOADER_SECTORS}
        conv=notrunc
    OUTPUT_QUIET
    ERROR_QUIET
    RESULT_VARIABLE DD_RESULT
)

if(NOT DD_RESULT EQUAL 0)
    message(FATAL_ERROR "dd command failed for kernel (result: ${DD_RESULT})")
endif()

# Get final image size
file(SIZE "${BOOT_IMG}" IMAGE_BYTES)
math(EXPR IMAGE_SECTORS "(${IMAGE_BYTES} + 511) / 512")

message(STATUS "")
message(STATUS "Boot image created: ${BOOT_IMG}")
message(STATUS "  Total size: ${IMAGE_BYTES} bytes (${IMAGE_SECTORS} sectors)")
message(STATUS "  Layout:")
message(STATUS "    Sector 1-${BOOTLOADER_SECTORS}: Bootloader (LBA 0-${BOOTLOADER_SECTORS}-1)")
message(STATUS "    Sector ${BOOTLOADER_SECTORS}+1-${IMAGE_SECTORS}: Kernel (LBA ${BOOTLOADER_SECTORS}-${IMAGE_SECTORS}-1)")
message(STATUS "")
