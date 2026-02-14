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
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

set_target_properties(kernel_main PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)

# 链接内核ELF
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/kernel.elf
    COMMAND ${CMAKE_LINKER} -m elf_x86_64
        -T ${CMAKE_SOURCE_DIR}/linker.ld
        -nostdlib
        -o ${CMAKE_BINARY_DIR}/kernel.elf
        ${CMAKE_BINARY_DIR}/kernel_entry.o
        $<TARGET_OBJECTS:kernel_main>
    DEPENDS
        ${CMAKE_BINARY_DIR}/kernel_entry.o
        kernel_main
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
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Kernel size:"
    COMMAND ls -lh ${CMAKE_BINARY_DIR}/kernel.bin
    DEPENDS ${CMAKE_BINARY_DIR}/kernel.elf
    COMMENT "Extracting kernel binary"
    VERBATIM
)

add_custom_target(kernel DEPENDS ${CMAKE_BINARY_DIR}/kernel.bin)
