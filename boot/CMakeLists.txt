# ============================================================================
# Bootloader 构建
# 说明：bootloader.asm 通过 %include 指令引用 boot/lib/ 下的文件
# NASM 会自动处理这些 include，CMake 只需编译主文件
# ============================================================================

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/bootloader.bin
    COMMAND ${NASM} -f bin
        ${CMAKE_CURRENT_SOURCE_DIR}/bootloader.asm
        -o ${CMAKE_BINARY_DIR}/bootloader.bin
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Bootloader size:"
    COMMAND ls -lh ${CMAKE_BINARY_DIR}/bootloader.bin
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/bootloader.asm
        # NASM 的依赖扫描会自动处理 %include 的文件
    COMMENT "Building bootloader (with %include dependencies)"
    VERBATIM
)

add_custom_target(bootloader DEPENDS ${CMAKE_BINARY_DIR}/bootloader.bin)
