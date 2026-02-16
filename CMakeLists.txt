cmake_minimum_required(VERSION 3.20)
project(CCOS_x64 ASM C)

# ============================================================================
# 导出 compile_commands.json 供 clangd 使用
# ============================================================================
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# 项目配置选项
# ============================================================================
option(CCOS_ENABLE_SERIAL "Enable serial port debug output" OFF)
option(CCOS_ENABLE_QEMU_DEBUG "Enable QEMU debug target by default" OFF)
option(CCOS_BUILD_TESTS "Build the static test suite" ON)
mark_as_advanced(CCOS_BUILD_TESTS)  # Hide from advanced UI, always on by default

# 设置构建类型（支持Debug/Release）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ============================================================================
# 工具链检测
# ============================================================================
find_program(NASM nasm REQUIRED)
find_program(QEMU qemu-system-x86_64 REQUIRED)
find_program(PYTHON python3 REQUIRED)

message(STATUS "Found NASM: ${NASM}")
message(STATUS "Found QEMU: ${QEMU}")
message(STATUS "Found Python: ${PYTHON}")

# ============================================================================
# 输出目录设置
# ============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================================
# 编译器标志设置
# ============================================================================
# 通用C编译器标志（所有目标共享）
add_compile_options(
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Wextra>
    $<$<COMPILE_LANGUAGE:C>:-Werror>
    $<$<COMPILE_LANGUAGE:C>:-std=c23>
    $<$<COMPILE_LANGUAGE:C>:-m64>
    $<$<COMPILE_LANGUAGE:C>:-mno-sse>
    $<$<COMPILE_LANGUAGE:C>:-mno-sse2>
)

# Debug/Release 特定标志（仅当未设置时才使用默认值）
if(NOT CMAKE_C_FLAGS_DEBUG)
    set(CMAKE_C_FLAGS_DEBUG "-g -O0")
endif()
if(NOT CMAKE_C_FLAGS_RELEASE)
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

# 打印当前构建类型的编译器标志
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "C flags for Debug build: ${CMAKE_C_FLAGS_DEBUG}")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "C flags for Release build: ${CMAKE_C_FLAGS_RELEASE}")
else()
    message(STATUS "C flags for ${CMAKE_BUILD_TYPE} build: ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE}}")
endif()

# ============================================================================
# Freestanding 环境编译标志（仅内核使用）
# ============================================================================
# 这些标志在 kernel/CMakeLists.txt 中应用到内核目标
set(CCOS_FREESTANDING_FLAGS
    -ffreestanding
    -fno-builtin
    -fno-stack-protector
    -nostdlib
    -nostdinc
    -nostartfiles
    -nodefaultlibs
    -mcmodel=large
)

# ============================================================================
# 项目配置头文件（可选，用于条件编译）
# ============================================================================
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/ccos_config.h.in
    ${CMAKE_BINARY_DIR}/include/ccos_config.h
)

# ============================================================================
# 子目录
# ============================================================================
add_subdirectory(boot)
add_subdirectory(kernel)

# ============================================================================
# 测试套件（可选，独立于内核构建）
# ============================================================================
# 测试在构建时自动运行，任何失败都会停止构建
if(CCOS_BUILD_TESTS)
    add_subdirectory(test)

    # 强制测试在任何内核构建目标之前通过
    # 这确保了静态检查不会把问题带到运行时
    # 注意：add_dependencies 调用需要在目标定义之后，见下方
endif()

# Two-stage build: final_config depends on bootloader, and then rebuilds bootloader
add_dependencies(final_config bootloader)

# After final_config is generated, rebuild bootloader with correct config
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/bootloader_final.bin
    COMMAND ${NASM} -f bin
        -I${CMAKE_SOURCE_DIR}/boot/
        ${CMAKE_SOURCE_DIR}/boot/bootloader.asm
        -o ${CMAKE_BINARY_DIR}/bootloader_final.bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/bootloader_final.bin
        ${CMAKE_BINARY_DIR}/bootloader.bin
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Final bootloader (with correct config):"
    COMMAND ls -lh ${CMAKE_BINARY_DIR}/bootloader.bin
    DEPENDS
        ${CMAKE_SOURCE_DIR}/boot/bootloader.asm
        ${CMAKE_SOURCE_DIR}/boot/boot_config.inc
        ${CMAKE_BINARY_DIR}/boot_config_final.inc
    COMMENT "Rebuilding bootloader with final config"
    VERBATIM
)

add_custom_target(bootloader_final DEPENDS ${CMAKE_BINARY_DIR}/bootloader_final.bin)
add_dependencies(bootloader_final final_config)

# ============================================================================
# 自定义目标：创建最终启动镜像
# ============================================================================
# Build order: bootloader -> kernel -> final_config -> bootloader_final -> boot_img
add_custom_target(boot_img ALL
    DEPENDS bootloader_final kernel final_config
    COMMAND ${CMAKE_COMMAND} -E echo ==========================================
    COMMAND ${CMAKE_COMMAND} -E echo "Creating boot image with dd..."
    COMMAND ${CMAKE_COMMAND} -E echo ==========================================
    # Create boot image by combining bootloader and kernel
    COMMAND ${CMAKE_COMMAND} -DBOOTLOADER_BIN=${CMAKE_BINARY_DIR}/bootloader.bin
        -DKERNEL_BIN=${CMAKE_BINARY_DIR}/kernel.bin
        -DBOOT_IMG=${CMAKE_BINARY_DIR}/boot.img
        -P ${CMAKE_SOURCE_DIR}/cmake/AssembleBootImage.cmake
    COMMENT "Creating boot image"
)

# 显示构建产物信息和验证
add_custom_command(TARGET boot_img POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Build artifacts:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ${CMAKE_BINARY_DIR}/bootloader.bin"
    COMMAND ${CMAKE_COMMAND} -E echo "  ${CMAKE_BINARY_DIR}/kernel.bin"
    COMMAND ${CMAKE_COMMAND} -E echo "  ${CMAKE_BINARY_DIR}/boot.img"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Verifying disk layout..."
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/verify_disk_layout.py
        ${CMAKE_BINARY_DIR}/bootloader.bin
        ${CMAKE_BINARY_DIR}/kernel.bin
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Verifying boot image integrity..."
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/verify_boot_image.py
        ${CMAKE_BINARY_DIR}/boot.img
        ${CMAKE_BINARY_DIR}/bootloader.bin
        ${CMAKE_BINARY_DIR}/kernel.bin
)

# ============================================================================
# 运行目标
# ============================================================================
add_custom_target(run
    COMMAND ${QEMU} -drive format=raw,file=${CMAKE_BINARY_DIR}/boot.img,if=ide -serial stdio
    DEPENDS boot_img
    COMMENT "Running CCOS in QEMU (text mode, no graphics)"
    VERBATIM
)

# VGA运行目标（图形界面 - VNC backend）
add_custom_target(vga-run
    COMMAND ${QEMU} -drive format=raw,file=${CMAKE_BINARY_DIR}/boot.img -serial stdio -vga std -display vnc=:0 
    DEPENDS boot_img
    COMMENT "Running CCOS in QEMU with VGA graphics (VNC backend, connect with: vncviewer localhost:5900)"
    VERBATIM
)

# 调试目标
add_custom_target(debug
    COMMAND ${QEMU} -drive format=raw,file=${CMAKE_BINARY_DIR}/boot.img,if=ide -serial stdio -nographic -s -S
    DEPENDS boot_img
    COMMENT "Running CCOS in QEMU (debug mode, connect with: gdb -ex 'target remote :1234')"
    VERBATIM
)

# ============================================================================
# 构建并运行目标（便捷快捷方式）
# ============================================================================
add_custom_target(build-and-vga-run
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "Building CCOS..."
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} --build . --target boot_img
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "Starting QEMU with VGA graphics (VNC)..."
    COMMAND ${CMAKE_COMMAND} -E echo "Connect with: vncviewer localhost:5900"
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${QEMU} -drive format=raw,file=${CMAKE_BINARY_DIR}/boot.img -vga std -display vnc=:0
    COMMENT "Build and run CCOS in QEMU with VGA graphics (VNC backend)"
    VERBATIM
)

# ============================================================================
# 安装目标（可选，用于生成ISO或部署）
# ============================================================================
install(FILES ${CMAKE_BINARY_DIR}/boot.img DESTINATION . OPTIONAL)

# ============================================================================
# 测试依赖关系（必须在所有目标定义之后）
# ============================================================================
# 强制测试在任何内核构建目标之前通过
if(CCOS_BUILD_TESTS)
    add_dependencies(kernel check)
    add_dependencies(bootloader check)
    add_dependencies(boot_img check)
    add_dependencies(bootloader_final check)
endif()
