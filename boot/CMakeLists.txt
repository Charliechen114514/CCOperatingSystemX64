# ============================================================================
# Bootloader 构建
# 说明：bootloader.asm 通过 %include 指令引用 boot/lib/ 下的文件
# NASM 会自动处理这些 include，CMake 只需编译主文件
# ============================================================================

# Initial bootloader build with mock config (for first build/size detection)
# The mock config in boot/boot_config.inc is used for this initial build
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/bootloader.bin
    COMMAND ${NASM} -f bin
        -I${CMAKE_CURRENT_SOURCE_DIR}/
        ${CMAKE_CURRENT_SOURCE_DIR}/bootloader.asm
        -o ${CMAKE_BINARY_DIR}/bootloader.bin
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Bootloader binary size:"
    COMMAND ls -lh ${CMAKE_BINARY_DIR}/bootloader.bin
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/bootloader.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/boot_config.inc
    COMMENT "Building bootloader (initial build with mock config)"
    VERBATIM
)

add_custom_target(bootloader DEPENDS ${CMAKE_BINARY_DIR}/bootloader.bin)
