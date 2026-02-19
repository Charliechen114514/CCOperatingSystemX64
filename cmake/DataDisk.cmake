# ==============================================================================
# CCOS Data Disk Configuration
# ==============================================================================
# Manages the creation and configuration of a data disk image with ext2
# filesystem for filesystem development. The disk is automatically created
# and formatted if it doesn't exist.
# ==============================================================================

# ============================================================================
# Configuration Options
# ============================================================================

# Default disk size (128MB)
set(CCOS_DATA_DISK_SIZE "128M" CACHE STRING "Size of data disk image (e.g., 32M, 128M, 512M, 1G)")

# Filesystem type for data disk (ext2 by default)
set(CCOS_DATA_DISK_FSTYPE "ext2" CACHE STRING "Filesystem type: ext2, ext3, ext4, or none (raw)")

# Optional: override default disk path
set(CCOS_DATA_DISK_PATH "" CACHE FILEPATH "Custom path to disk image (overrides default disk/disk.img)")

# ============================================================================
# Determine Disk Path
# ============================================================================

# Default path: project root/disk/disk.img
if(CCOS_DATA_DISK_PATH)
    set(CCOS_DATA_DISK_IMAGE "${CCOS_DATA_DISK_PATH}")
    message(STATUS "Using custom data disk path: ${CCOS_DATA_DISK_IMAGE}")
else()
    set(CCOS_DATA_DISK_IMAGE "${CMAKE_SOURCE_DIR}/disk/disk.img")
    message(STATUS "Using default data disk path: ${CCOS_DATA_DISK_IMAGE}")
endif()

# ============================================================================
# Create Disk Image (if needed)
# ============================================================================

# Ensure the disk directory exists
get_filename_component(CCOS_DATA_DISK_DIR "${CCOS_DATA_DISK_IMAGE}" DIRECTORY)
file(MAKE_DIRECTORY "${CCOS_DATA_DISK_DIR}")

# Check if disk image already exists
if(EXISTS "${CCOS_DATA_DISK_IMAGE}")
    # Get the actual size of existing disk
    file(SIZE "${CCOS_DATA_DISK_IMAGE}" CCOS_DISK_BYTES)

    # Convert to human readable format
    if(CCOS_DISK_BYTES GREATER 1073741824)
        math(EXPR CCOS_DISK_SIZE_GB "${CCOS_DISK_BYTES} / 1073741824")
        set(CCOS_DISK_SIZE_HUMAN "${CCOS_DISK_SIZE_GB}G")
    elseif(CCOS_DISK_BYTES GREATER 1048576)
        math(EXPR CCOS_DISK_SIZE_MB "${CCOS_DISK_BYTES} / 1048576")
        set(CCOS_DISK_SIZE_HUMAN "${CCOS_DISK_SIZE_MB}M")
    else()
        math(EXPR CCOS_DISK_SIZE_KB "${CCOS_DISK_BYTES} / 1024")
        set(CCOS_DISK_SIZE_HUMAN "${CCOS_DISK_SIZE_KB}K")
    endif()

    message(STATUS "Data disk already exists: ${CCOS_DATA_DISK_IMAGE} (${CCOS_DISK_SIZE_HUMAN})")
    set(CCOS_DATA_DISK_EXISTS TRUE)
else()
    # Parse size string (e.g., "128M" -> 128 MB, "1G" -> 1024 MB)
    if(CCOS_DATA_DISK_SIZE MATCHES "^([0-9]+)([MG])$")
        set(CCOS_DISK_SIZE_NUM "${CMAKE_MATCH_1}")
        set(CCOS_DISK_SIZE_UNIT "${CMAKE_MATCH_2}")

        if(CCOS_DISK_SIZE_UNIT STREQUAL "G")
            # Convert GB to MB for dd
            math(EXPR CCOS_DISK_COUNT "${CCOS_DISK_SIZE_NUM} * 1024")
            set(CCOS_DISK_BS "1M")
            set(CCOS_DISK_SIZE_HUMAN "${CCOS_DISK_SIZE_NUM}G")
        else()
            set(CCOS_DISK_COUNT "${CCOS_DISK_SIZE_NUM}")
            set(CCOS_DISK_BS "1M")
            set(CCOS_DISK_SIZE_HUMAN "${CCOS_DISK_SIZE_NUM}M")
        endif()
    else()
        message(FATAL_ERROR "Invalid CCOS_DATA_DISK_SIZE format: '${CCOS_DATA_DISK_SIZE}'. Use format like '128M' or '1G'")
    endif()

    # Create the disk image using dd
    message(STATUS "Creating data disk: ${CCOS_DATA_DISK_IMAGE} (${CCOS_DISK_SIZE_HUMAN})")

    execute_process(
        COMMAND dd if=/dev/zero
                  of=${CCOS_DATA_DISK_IMAGE}
                  bs=${CCOS_DISK_BS}
                  count=${CCOS_DISK_COUNT}
        OUTPUT_VARIABLE DD_OUTPUT
        ERROR_VARIABLE DD_ERROR
        RESULT_VARIABLE DD_RESULT
    )

    if(NOT DD_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to create data disk image: ${DD_ERROR}")
    endif()

    # Parse dd output to get actual size
    if(DD_OUTPUT MATCHES "copied, ([0-9.]+ [A-Z]+)")
        set(CCOS_DISK_ACTUAL_SIZE "${CMAKE_MATCH_1}")
    else()
        set(CCOS_DISK_ACTUAL_SIZE "${CCOS_DISK_SIZE_HUMAN}")
    endif()

    message(STATUS "Data disk created successfully: ${CCOS_DISK_ACTUAL_SIZE}")

    # Format the disk with ext2 filesystem
    if(NOT CCOS_DATA_DISK_FSTYPE STREQUAL "none")
        message(STATUS "Formatting data disk with ${CCOS_DATA_DISK_FSTYPE} filesystem...")

        # Find mkfs command
        find_program(MKFS_EXT2 mkfs.ext2)
        find_program(MKFS_EXT3 mkfs.ext3)
        find_program(MKFS_EXT4 mkfs.ext4)

        set(MKFS_CMD "")
        if(CCOS_DATA_DISK_FSTYPE STREQUAL "ext2" AND MKFS_EXT2)
            set(MKFS_CMD "${MKFS_EXT2} -F ${CCOS_DATA_DISK_IMAGE}")
        elseif(CCOS_DATA_DISK_FSTYPE STREQUAL "ext3" AND MKFS_EXT3)
            set(MKFS_CMD "${MKFS_EXT3} -F ${CCOS_DATA_DISK_IMAGE}")
        elseif(CCOS_DATA_DISK_FSTYPE STREQUAL "ext4" AND MKFS_EXT4)
            set(MKFS_CMD "${MKFS_EXT4} -F ${CCOS_DATA_DISK_IMAGE}")
        else()
            message(WARNING "mkfs.${CCOS_DATA_DISK_FSTYPE} not found, skipping filesystem format")
        endif()

        if(MKFS_CMD)
            # Use bash -c to properly execute the command string with arguments
            execute_process(
                COMMAND bash -c "${MKFS_CMD}"
                OUTPUT_VARIABLE MKFS_OUTPUT
                ERROR_VARIABLE MKFS_ERROR
                RESULT_VARIABLE MKFS_RESULT
            )

            if(MKFS_RESULT EQUAL 0)
                message(STATUS "Data disk formatted successfully with ${CCOS_DATA_DISK_FSTYPE}")
            else()
                message(FATAL_ERROR "Failed to format data disk: ${MKFS_ERROR}")
            endif()
        endif()
    else()
        message(STATUS "Skipping filesystem format (fstype=none)")
    endif()

    set(CCOS_DATA_DISK_EXISTS FALSE)
endif()

# ============================================================================
# Print Configuration Summary
# ============================================================================

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Data Disk Configuration:")
message(STATUS "  Path:   ${CCOS_DATA_DISK_IMAGE}")
message(STATUS "  Size:   ${CCOS_DISK_SIZE_HUMAN}")
message(STATUS "  FS:     ${CCOS_DATA_DISK_FSTYPE}")
message(STATUS "  QEMU:   -drive format=raw,file=${CCOS_DATA_DISK_IMAGE},if=ide,index=1")
message(STATUS "========================================")
message(STATUS "")
