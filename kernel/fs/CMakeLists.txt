# ============================================================================
# CCOS - Filesystem Subsystem
# ============================================================================

cmake_minimum_required(VERSION 3.20)

add_library(fs STATIC)

target_include_directories(fs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/block
    ${CMAKE_CURRENT_SOURCE_DIR}/vfs
    ${CMAKE_CURRENT_SOURCE_DIR}/ext2
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(fs PUBLIC ${CCOS_FREESTANDING_FLAGS})

# Source files - Phase 1 & 2: Block device and VFS core
# Phase 3: EXT2 filesystem driver
target_sources(fs PRIVATE
    # Main filesystem
    fs.c

    # Block device layer
    block/block.c

    # VFS core
    vfs/vfs_superblock.c
    vfs/vfs_inode.c
    vfs/vfs_file.c
    vfs/vfs_dentry.c
    vfs/vfs_open.c
    vfs/vfs_mount.c
    vfs/vfs_path.c

    # EXT2 filesystem driver
    ext2/ext2.c
    ext2/ext2_superblock.c
    ext2/ext2_inode.c
    ext2/ext2_file.c
    ext2/ext2_dir.c
)

# Link dependencies
target_link_libraries(fs PUBLIC
    base
    list
    bitmap
    heap
    klogs
    driver
    interrupt
)

set_target_properties(fs PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
