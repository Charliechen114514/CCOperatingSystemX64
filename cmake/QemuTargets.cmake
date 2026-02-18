# ==============================================================================
# CCOS - QEMU 运行目标配置
# ==============================================================================
# 此文件定义所有 QEMU 相关的运行目标
# 内存大小通过 CCOS_QEMU_MEMORY_ARG 变量配置（来自 MemSizeConfig.cmake）
# ==============================================================================

# ============================================================================
# 运行目标
# ============================================================================

# 基础运行目标（文本模式，串口输出）
add_custom_target(run
    COMMAND ${QEMU} -m ${CCOS_QEMU_MEMORY_ARG}
        -drive format=raw,file=${CMAKE_BINARY_DIR}/boot.img,if=ide
        -serial stdio
    DEPENDS boot_img
    COMMENT "Running CCOS in QEMU with ${CCOS_MEMORY_SIZE_HUMAN} memory (text mode)"
    VERBATIM
)

# VGA运行目标（图形界面 - VNC backend）
add_custom_target(vga-run
    COMMAND ${QEMU} -m ${CCOS_QEMU_MEMORY_ARG}
        -drive format=raw,file=${CMAKE_BINARY_DIR}/boot.img
        -serial stdio
        -vga std
        -display vnc=:0
    DEPENDS boot_img
    COMMENT "Running CCOS in QEMU with VGA graphics (VNC: vncviewer localhost:5900)"
    VERBATIM
)

# QEMU Monitor 运行目标（Monitor 通过 telnet localhost:4444 访问）
add_custom_target(qemu-monitor-run
    COMMAND ${QEMU} -m ${CCOS_QEMU_MEMORY_ARG}
        -drive format=raw,file=${CMAKE_BINARY_DIR}/boot.img
        -serial stdio
        -monitor telnet:127.0.0.1:4444,server,nowait
    DEPENDS boot_img
    COMMENT "Running CCOS with Monitor on telnet port 4444"
    VERBATIM
)

# 调试目标
add_custom_target(debug
    COMMAND ${QEMU} -m ${CCOS_QEMU_MEMORY_ARG}
        -drive format=raw,file=${CMAKE_BINARY_DIR}/boot.img,if=ide
        -serial stdio
        -nographic
        -s -S
    DEPENDS boot_img
    COMMENT "Running CCOS in debug mode (gdb: target remote :1234)"
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
    COMMAND ${CMAKE_COMMAND} -E echo "Memory: ${CCOS_MEMORY_SIZE_HUMAN}"
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${QEMU} -m ${CCOS_QEMU_MEMORY_ARG}
        -drive format=raw,file=${CMAKE_BINARY_DIR}/boot.img
        -vga std
        -display vnc=:0
    COMMENT "Build and run CCOS with VGA graphics"
    VERBATIM
)

add_custom_target(build-and-qemu-monitor-run
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "Building CCOS..."
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} --build . --target boot_img
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "Starting QEMU with Monitor..."
    COMMAND ${CMAKE_COMMAND} -E echo "Monitor: telnet localhost 4444"
    COMMAND ${CMAKE_COMMAND} -E echo "Memory: ${CCOS_MEMORY_SIZE_HUMAN}"
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${QEMU} -m ${CCOS_QEMU_MEMORY_ARG}
        -drive format=raw,file=${CMAKE_BINARY_DIR}/boot.img
        -serial stdio
        -monitor telnet:127.0.0.1:4444,server,nowait
    COMMENT "Build and run CCOS with QEMU Monitor"
    VERBATIM
)

# ============================================================================
# 额外的便捷目标
# ============================================================================

# 仅运行（不重新构建）
add_custom_target(run-only
    COMMAND ${QEMU} -m ${CCOS_QEMU_MEMORY_ARG}
        -drive format=raw,file=${CMAKE_BINARY_DIR}/boot.img,if=ide
        -serial stdio
    DEPENDS boot_img
    COMMENT "Running CCOS without rebuild"
    VERBATIM
)
