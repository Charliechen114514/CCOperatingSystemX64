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

# 中断汇编存根文件
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/interrupt_asm.o
    COMMAND ${NASM} -f elf64
        ${CMAKE_CURRENT_SOURCE_DIR}/interrupt/interrupt.asm
        -o ${CMAKE_BINARY_DIR}/interrupt_asm.o
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/interrupt/interrupt.asm
    COMMENT "Building interrupt assembly stubs"
    VERBATIM
)

# C内核主文件
# 使用 OBJECT 库类型便于未来扩展
add_library(kernel_main OBJECT
    kernel_main.c
    kernel_init.c
)

target_include_directories(kernel_main PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/defines
    ${CMAKE_BINARY_DIR}/include
)

# 应用 freestanding 环境特定的编译标志
target_compile_options(kernel_main PUBLIC ${CCOS_FREESTANDING_FLAGS})

# 如果启用了帧指针选项，给所有目标添加 -fno-omit-frame-pointer 以支持栈回溯
if(CCOS_ENABLE_FRAME_POINTER)
    # 为 kernel 主目标添加帧指针
    target_compile_options(kernel_main PUBLIC -fno-omit-frame-pointer)
    message(STATUS "Frame pointer enabled for stack tracing support")

    # 创建一个全局的编译选项变量，供所有子目标使用
    set(CCOS_FRAME_POINTER_FLAGS -fno-omit-frame-pointer CACHE INTERNAL "Flags for frame pointer support")
endif()

set_target_properties(kernel_main PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)

# 添加 assert 模块
add_subdirectory(assert)

# 添加 base 基础库模块
add_subdirectory(base)

# 添加 math 数学模块
add_subdirectory(math)

# 添加 bitmap 位图模块
add_subdirectory(bitmap)

# 添加 list 嵌入式链表模块
add_subdirectory(list)

# 添加驱动子目录
add_subdirectory(driver)

# 添加 klogs 日志模块
add_subdirectory(klogs)

# 添加 welcomes 欢迎界面模块
add_subdirectory(welcomes)

# 添加 stacktrace 栈回溯模块
add_subdirectory(stacktrace)

# 添加 interrupt 中断子系统模块
add_subdirectory(interrupt)

# 添加 demo 演示模块（可选）
# 定义各个 demo 的 option（需要在 add_subdirectory 之前定义，以便可以通过命令行设置）
option(ENABLE_RTC_DEMO "Enable RTC driver demo" OFF)
option(ENABLE_VMM_DEMO "Enable VMM demo" OFF)

# 自动启用 CCOS_ENABLE_DEMO 如果任何子 demo 被启用
if(ENABLE_RTC_DEMO OR ENABLE_VMM_DEMO)
    set(CCOS_ENABLE_DEMO ON CACHE BOOL "Enable kernel demo programs" FORCE)
else()
    option(CCOS_ENABLE_DEMO "Enable kernel demo programs" OFF)
endif()

# 始终添加 demo 子目录（基础 demo 库必须始终存在以提供 run_possible_demos 符号）
add_subdirectory(demo)

# 添加 shell 模块
add_subdirectory(shell)

# 添加 mm 内存管理模块
add_subdirectory(mm)

# Demo library is always built and linked (provides run_possible_demos symbol)
# Collect all sub-demo libraries for explicit linking (static libs don't propagate deps)
set(KERNEL_DEMO_LIBS $<TARGET_FILE:demo>)
set(KERNEL_DEMO_DEPENDS demo)
if(TARGET demo_rtc)
    list(APPEND KERNEL_DEMO_LIBS $<TARGET_FILE:demo_rtc>)
    list(APPEND KERNEL_DEMO_DEPENDS demo_rtc)
endif()
if(TARGET demo_vmm)
    list(APPEND KERNEL_DEMO_LIBS $<TARGET_FILE:demo_vmm>)
    list(APPEND KERNEL_DEMO_DEPENDS demo_vmm)
endif()

# 链接内核ELF
# Note: Library order matters for traditional linkers - libraries must come AFTER
# the object files that reference their symbols
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/kernel.elf
    COMMAND ${CMAKE_LINKER} -m elf_x86_64
        -T ${CMAKE_SOURCE_DIR}/linker.ld
        -nostdlib
        --gc-sections
        -no-pie
        -z noexecstack
        -o ${CMAKE_BINARY_DIR}/kernel.elf
        ${CMAKE_BINARY_DIR}/kernel_entry.o
        ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/kernel_main.dir/kernel_main.c.o
        ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/kernel_main.dir/kernel_init.c.o
        # interrupt assembly stubs
        ${CMAKE_BINARY_DIR}/interrupt_asm.o
        # klogs object files
        ${CMAKE_CURRENT_BINARY_DIR}/klogs/CMakeFiles/klogs.dir/kprintf.c.o
        ${CMAKE_CURRENT_BINARY_DIR}/klogs/CMakeFiles/klogs.dir/kprintf_backends.c.o
        ${CMAKE_CURRENT_BINARY_DIR}/klogs/CMakeFiles/klogs.dir/backends/serial_backends.c.o
        # welcomes object files
        ${CMAKE_CURRENT_BINARY_DIR}/welcomes/CMakeFiles/welcomes.dir/welcome.c.o
        ${CMAKE_CURRENT_BINARY_DIR}/welcomes/CMakeFiles/welcomes.dir/vga_welcomes.c.o
        ${CMAKE_CURRENT_BINARY_DIR}/welcomes/CMakeFiles/welcomes.dir/serial_welcome.c.o
        # interrupt object files
        ${CMAKE_CURRENT_BINARY_DIR}/interrupt/CMakeFiles/interrupt.dir/idt.c.o
        ${CMAKE_CURRENT_BINARY_DIR}/interrupt/CMakeFiles/interrupt.dir/interrupt.c.o
        # Libraries must come AFTER object files that reference them
        # Use --start-group/--end-group to handle circular dependencies
        # (assert depends on base/driver, but base also calls assert functions)
        --start-group
        $<TARGET_FILE:assert>
        $<TARGET_FILE:base>
        $<TARGET_FILE:bitmap>
        $<TARGET_FILE:list>
        $<TARGET_FILE:driver>
        $<TARGET_FILE:stacktrace>
        $<TARGET_FILE:memory_detect>
        $<TARGET_FILE:pframe>
        $<TARGET_FILE:kernel_vmm>
        # Demo libraries (base + any enabled sub-demos)
        ${KERNEL_DEMO_LIBS}
        --end-group
        # Shell library
        $<TARGET_FILE:shell>
    DEPENDS
        ${CMAKE_BINARY_DIR}/kernel_entry.o
        ${CMAKE_BINARY_DIR}/interrupt_asm.o
        kernel_main
        assert
        base
        math
        bitmap
        list
        driver
        klogs
        welcomes
        stacktrace
        interrupt
        ${KERNEL_DEMO_DEPENDS}
        shell
        memory_detect
        pframe
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
# 两阶段符号提取（避免竞态条件）
# ============================================================================
# 使用 POST_BUILD + stamp 文件模式来实现确定性的两阶段构建
#
# 工作原理：
# 1. kernel 目标完成后，提取符号到 stacktrace/symbols.c
# 2. symbols.c 是 stacktrace 库的源文件，会触发重新编译
# 3. kernel.elf 需要重新链接以包含新的符号
#
# 关键：使用 .stamp 文件来追踪状态，避免递归 cmake --build

# 符号提取的 stamp 文件（位于构建目录，避免污染源码）
set(SYMBOLS_EXTRACTED_STAMP ${CMAKE_CURRENT_BINARY_DIR}/.symbols_extracted)

# 符号提取命令：在 kernel 构建后自动执行
# 使用 POST_BUILD 确保每次 kernel.elf 更新后都会重新提取符号
add_custom_command(
    TARGET kernel
    POST_BUILD
    COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/extract_symbols.py
        ${CMAKE_BINARY_DIR}/kernel.elf
        -o ${CMAKE_CURRENT_SOURCE_DIR}/stacktrace
    COMMAND ${CMAKE_COMMAND} -E touch ${SYMBOLS_EXTRACTED_STAMP}
    COMMENT "Extracting symbols from kernel.elf (triggering rebuild if changed)"
    VERBATIM
)

# ============================================================================
# Kernel Size and Memory Report
# ============================================================================
add_custom_command(
    TARGET kernel
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DKERNEL_BIN=${CMAKE_BINARY_DIR}/kernel.bin
        -DCCOS_MEMORY_SIZE_MB=${CCOS_MEMORY_SIZE_MB}
        -DCCOS_MEMORY_SIZE_HUMAN=${CCOS_MEMORY_SIZE_HUMAN}
        -P ${CMAKE_SOURCE_DIR}/cmake/KernelSizeReport.cmake
    COMMENT "Generating kernel size and memory report"
    VERBATIM
)

# ============================================================================
# kernel_with_symbols: 确保符号被包含的最终 kernel
# ============================================================================
# 这个目标依赖于符号提取的 stamp 文件
# 当 symbols.c 变化时，stacktrace 库会重新编译
# 这会触发 kernel.elf 的重新链接（因为 kernel 依赖 stacktrace）
#
# 注意：这里不需要递归的 cmake --build
# CMake 的依赖系统会自动处理重新编译/链接
add_custom_target(kernel_with_symbols
    COMMAND ${CMAKE_COMMAND} -E echo "=== Kernel built with up-to-date symbols ==="
    DEPENDS ${SYMBOLS_EXTRACTED_STAMP} kernel
    COMMENT "Final kernel with symbols"
    VERBATIM
)

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
