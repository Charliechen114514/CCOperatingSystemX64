cmake_minimum_required(VERSION 3.20)
project(CCOS_x64 ASM C)

# ============================================================================
# 项目配置选项
# ============================================================================
option(CCOS_ENABLE_SERIAL "Enable serial port debug output" OFF)
option(CCOS_ENABLE_QEMU_DEBUG "Enable QEMU debug target by default" OFF)

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
# C编译器标志（freestanding环境）
add_compile_options(
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Wextra>
    $<$<COMPILE_LANGUAGE:C>:-Werror>
    $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
    $<$<COMPILE_LANGUAGE:C>:-fno-builtin>
    $<$<COMPILE_LANGUAGE:C>:-fno-stack-protector>
    $<$<COMPILE_LANGUAGE:C>:-nostdlib>
    $<$<COMPILE_LANGUAGE:C>:-nostdinc>
    $<$<COMPILE_LANGUAGE:C>:-nostartfiles>
    $<$<COMPILE_LANGUAGE:C>:-nodefaultlibs>
    $<$<COMPILE_LANGUAGE:C>:-mcmodel=large>
    $<$<COMPILE_LANGUAGE:C>:-m64>
)

# Debug/Release 特定标志
set(CMAKE_C_FLAGS_DEBUG "-g -O0")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

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

# Explicit dependency: final_config needs bootloader
add_dependencies(final_config bootloader)

# ============================================================================
# 自定义目标：创建最终启动镜像
# ============================================================================
# Build order: bootloader -> kernel -> final_config -> boot_img
add_custom_target(boot_img ALL
    DEPENDS bootloader kernel final_config
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
    COMMAND ${QEMU} -drive format=raw,file=${CMAKE_BINARY_DIR}/boot.img,if=ide -nographic
    DEPENDS boot_img
    COMMENT "Running CCOS in QEMU (text mode, no graphics)"
    VERBATIM
)

# VGA运行目标（图形界面 - VNC backend）
add_custom_target(vga-run
    COMMAND ${QEMU} -drive format=raw,file=${CMAKE_BINARY_DIR}/boot.img -vga std -display vnc=:0
    DEPENDS boot_img
    COMMENT "Running CCOS in QEMU with VGA graphics (VNC backend, connect with: vncviewer localhost:5900)"
    VERBATIM
)

# 调试目标
add_custom_target(debug
    COMMAND ${QEMU} -drive format=raw,file=${CMAKE_BINARY_DIR}/boot.img,if=ide -nographic -s -S
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
