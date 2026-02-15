# ============================================================================
# Bootloader 构建
# ============================================================================

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
    COMMENT "Building bootloader"
    VERBATIM
)

add_custom_target(bootloader DEPENDS ${CMAKE_BINARY_DIR}/bootloader.bin)
