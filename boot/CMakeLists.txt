# ============================================================================
# Bootloader 构建
# 说明：bootloader.asm 通过 %include 指令引用 boot/lib/ 下的文件
# NASM 会自动处理这些 include，CMake 只需编译主文件
# ============================================================================

# Set bootloader size (must be determined after first build)
set(BOOTLOADER_SECTORS 3 CACHE STRING "Number of sectors bootloader occupies")

# Create boot_config.inc generation target
# This is a two-stage process:
# Stage 1: Generate initial config with mock values (for first build)
# Stage 2: After kernel is built, regenerate with actual sizes

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/boot_config.inc
    COMMAND ${CMAKE_COMMAND} -E echo "Generating boot_config.inc..."
    # For first build, copy the mock config
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/boot_config.inc
        ${CMAKE_CURRENT_BINARY_DIR}/boot_config.inc
    COMMENT "Generating initial boot configuration"
    VERBATIM
)

# Bootloader build depends on config
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/bootloader.bin
    COMMAND ${NASM} -f bin
        -I${CMAKE_CURRENT_BINARY_DIR}/
        ${CMAKE_CURRENT_SOURCE_DIR}/bootloader.asm
        -o ${CMAKE_BINARY_DIR}/bootloader.bin
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Bootloader size:"
    COMMAND ls -lh ${CMAKE_BINARY_DIR}/bootloader.bin
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/bootloader.asm
        ${CMAKE_CURRENT_BINARY_DIR}/boot_config.inc
        ${CMAKE_CURRENT_SOURCE_DIR}/boot_config.inc
    COMMENT "Building bootloader (with dynamic configuration)"
    VERBATIM
)

add_custom_target(bootloader DEPENDS
    ${CMAKE_BINARY_DIR}/bootloader.bin
    ${CMAKE_CURRENT_BINARY_DIR}/boot_config.inc
)
