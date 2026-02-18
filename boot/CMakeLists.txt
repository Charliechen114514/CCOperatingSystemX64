# ============================================================================
# Bootloader 构建
# ============================================================================

# 生成初始 mock 配置文件（用于首次构建，假设一个较大的内核）
# 这个值会被后续的 final_config 覆盖
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "; ==============================================================================" > ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "; CCOS Bootloader Configuration" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "; ==============================================================================" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "; Mock configuration for initial build" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "; This will be regenerated with actual sizes" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "; ==============================================================================" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "%define KERNEL_LBA_START          6" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "%define KERNEL_SECTOR_COUNT       512" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "%define KERNEL_LOAD_SEGMENT       0x1000" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "%define KERNEL_LOAD_OFFSET        0x0000" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "%define BOOTLOADER_SECTORS        6" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "%define KERNEL_MAX_SECTORS        131072" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "%define SECTORS_PER_TRACK         63" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "%define HEADS                     16" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "%define KERNEL_CHS_CYLINDER       0" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "%define KERNEL_CHS_HEAD           0" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND} -E echo "%define KERNEL_CHS_SECTOR         6" >> ${CMAKE_BINARY_DIR}/boot_config_final.inc
    VERBATIM
)

add_custom_target(mock_config DEPENDS ${CMAKE_BINARY_DIR}/boot_config_final.inc)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/bootloader.bin
    COMMAND ${NASM} -f bin
        -I${CMAKE_CURRENT_SOURCE_DIR}/
        -I${CMAKE_BINARY_DIR}/
        ${CMAKE_CURRENT_SOURCE_DIR}/bootloader.asm
        -o ${CMAKE_BINARY_DIR}/bootloader.bin
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Bootloader binary size:"
    COMMAND ls -lh ${CMAKE_BINARY_DIR}/bootloader.bin
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/bootloader.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/serial_constants.inc
        ${CMAKE_BINARY_DIR}/mem_detect_constants.inc
        ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMENT "Building bootloader"
    VERBATIM
)

add_custom_target(bootloader DEPENDS ${CMAKE_BINARY_DIR}/bootloader.bin mock_config)
