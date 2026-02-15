# ============================================================================
# 内核构建
# ============================================================================

# 汇编入口文件
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/kernel_entry.o
    COMMAND ${NASM} -f elf64
        ${CMAKE_CURRENT_SOURCE_DIR}/kernel_entry.asm
        -o ${CMAKE_BINARY_DIR}/kernel_entry.o
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/kernel_entry.asm
    COMMENT "Building kernel entry"
    VERBATIM
)

# C内核主文件
# 使用 OBJECT 库类型便于未来扩展
add_library(kernel_main OBJECT
    kernel_main.c
)

target_include_directories(kernel_main PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/defines
    ${CMAKE_BINARY_DIR}/include
)

# 应用 freestanding 环境特定的编译标志
target_compile_options(kernel_main PUBLIC ${CCOS_FREESTANDING_FLAGS})

set_target_properties(kernel_main PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)

# 添加驱动子目录
add_subdirectory(driver)

# 链接内核ELF
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/kernel.elf
    COMMAND ${CMAKE_LINKER} -m elf_x86_64
        -T ${CMAKE_SOURCE_DIR}/linker.ld
        -nostdlib
        -o ${CMAKE_BINARY_DIR}/kernel.elf
        ${CMAKE_BINARY_DIR}/kernel_entry.o
        $<TARGET_OBJECTS:kernel_main>
        $<TARGET_FILE:driver>
    DEPENDS
        ${CMAKE_BINARY_DIR}/kernel_entry.o
        kernel_main
        driver
        ${CMAKE_SOURCE_DIR}/linker.ld
    COMMENT "Linking kernel ELF"
    VERBATIM
)

# 提取内核二进制
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/kernel.bin
    COMMAND ${CMAKE_OBJCOPY} -O binary
        ${CMAKE_BINARY_DIR}/kernel.elf
        ${CMAKE_BINARY_DIR}/kernel.bin
    DEPENDS ${CMAKE_BINARY_DIR}/kernel.elf
    COMMENT "Extracting kernel binary"
    VERBATIM
)

add_custom_target(kernel DEPENDS ${CMAKE_BINARY_DIR}/kernel.bin)

# ============================================================================
# 自动生成 boot_config.inc（两阶段构建）
# ============================================================================
# 阶段1：生成初始配置（用于首次编译）
# 阶段2：在bootloader和kernel都构建后，生成实际配置，然后重新编译bootloader
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMAND ${CMAKE_COMMAND}
        -DKERNEL_BIN=${CMAKE_BINARY_DIR}/kernel.bin
        -DOUTPUT_FILE=${CMAKE_BINARY_DIR}/boot_config_final.inc
        -DBOOTLOADER_BIN=${CMAKE_BINARY_DIR}/bootloader.bin
        -P ${CMAKE_SOURCE_DIR}/cmake/GenerateKernelSize.cmake
    # Copy final config to boot directory to trigger bootloader rebuild
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/boot_config_final.inc
        ${CMAKE_SOURCE_DIR}/boot/boot_config.inc
    DEPENDS
        ${CMAKE_BINARY_DIR}/kernel.bin
        ${CMAKE_BINARY_DIR}/bootloader.bin
        ${CMAKE_SOURCE_DIR}/cmake/GenerateKernelSize.cmake
    COMMENT "Generating final boot_config.inc with actual sizes"
    VERBATIM
)

add_custom_target(final_config DEPENDS ${CMAKE_BINARY_DIR}/boot_config_final.inc)
