# ============================================================================
# Demo Programs - Optional demonstration code for kernel subsystems
# ============================================================================

# Track enabled demos
set(DEMO_ENABLED_LIST "")

# ----------------------------------------------------------------------------
# 基础 demo 库：始终构建，提供 run_possible_demos 符号
# ----------------------------------------------------------------------------
add_library(demo STATIC all_demo.c)

target_include_directories(demo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(demo PUBLIC ${CCOS_FREESTANDING_FLAGS})

# 基础 demo 库依赖 klogs（用于 klog_trace）
target_link_libraries(demo PUBLIC klogs)

set_target_properties(demo PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)

# ----------------------------------------------------------------------------
# 子 demo 库：根据选项构建
# ----------------------------------------------------------------------------

# RTC Demo
if(ENABLE_RTC_DEMO AND CCOS_ENABLE_DEMO)
    list(APPEND DEMO_ENABLED_LIST "RTC")
    message(STATUS "RTC Demo: ENABLED")

    add_library(demo_rtc STATIC rtc/rtc_demo.c)

    target_include_directories(demo_rtc PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_BINARY_DIR}/include
    )

    target_compile_options(demo_rtc PUBLIC ${CCOS_FREESTANDING_FLAGS})
    target_compile_definitions(demo_rtc PUBLIC RTC_DEMO_ENABLED=1)

    # RTC demo 依赖 driver 和 klogs
    target_link_libraries(demo_rtc PUBLIC driver klogs)

    set_target_properties(demo_rtc PROPERTIES
        POSITION_INDEPENDENT_CODE FALSE
    )

    # 将 RTC demo 链接到基础 demo 库
    target_link_libraries(demo PUBLIC demo_rtc)
    # 将宏定义传递到基础 demo 库
    target_compile_definitions(demo PUBLIC RTC_DEMO_ENABLED=1)
else()
    message(STATUS "RTC Demo: DISABLED")
endif()

# VMM Demo
if(ENABLE_VMM_DEMO AND CCOS_ENABLE_DEMO)
    list(APPEND DEMO_ENABLED_LIST "VMM")
    message(STATUS "VMM Demo: ENABLED")

    add_library(demo_vmm STATIC vmm/vmm_demo.c)

    target_include_directories(demo_vmm PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_BINARY_DIR}/include
    )

    target_compile_options(demo_vmm PUBLIC ${CCOS_FREESTANDING_FLAGS})
    target_compile_definitions(demo_vmm PUBLIC VMM_DEMO_ENABLED=1)

    # VMM demo 依赖 driver, klogs 和 kernel_vmm
    target_link_libraries(demo_vmm PUBLIC driver klogs kernel_vmm)

    set_target_properties(demo_vmm PROPERTIES
        POSITION_INDEPENDENT_CODE FALSE
    )

    # 将 VMM demo 链接到基础 demo 库
    target_link_libraries(demo PUBLIC demo_vmm)
    # 将宏定义传递到基础 demo 库
    target_compile_definitions(demo PUBLIC VMM_DEMO_ENABLED=1)
else()
    message(STATUS "VMM Demo: DISABLED")
endif()

# Heap Demo
if(ENABLE_HEAP_DEMO AND CCOS_ENABLE_DEMO)
    list(APPEND DEMO_ENABLED_LIST "HEAP")
    message(STATUS "Heap Demo: ENABLED")

    add_library(demo_heap STATIC heap/heap_demo.c)

    target_include_directories(demo_heap PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_BINARY_DIR}/include
    )

    target_compile_options(demo_heap PUBLIC ${CCOS_FREESTANDING_FLAGS})
    target_compile_definitions(demo_heap PUBLIC HEAP_DEMO_ENABLED=1)

    # Heap demo 依赖 klogs 和 kernel_heap
    target_link_libraries(demo_heap PUBLIC klogs kernel_heap base)

    set_target_properties(demo_heap PROPERTIES
        POSITION_INDEPENDENT_CODE FALSE
    )

    # 将 Heap demo 链接到基础 demo 库
    target_link_libraries(demo PUBLIC demo_heap)
    # 将宏定义传递到基础 demo 库
    target_compile_definitions(demo PUBLIC HEAP_DEMO_ENABLED=1)
else()
    message(STATUS "Heap Demo: DISABLED")
endif()

# COW Demo
if(ENABLE_COW_DEMO AND CCOS_ENABLE_DEMO)
    list(APPEND DEMO_ENABLED_LIST "COW")
    message(STATUS "COW Demo: ENABLED")

    add_library(demo_cow STATIC cow/cow_demo.c)

    target_include_directories(demo_cow PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_BINARY_DIR}/include
    )

    target_compile_options(demo_cow PUBLIC ${CCOS_FREESTANDING_FLAGS})
    target_compile_definitions(demo_cow PUBLIC COW_DEMO_ENABLED=1)

    # COW demo 依赖 klogs, kernel_vmm, base
    target_link_libraries(demo_cow PUBLIC klogs kernel_vmm base)

    set_target_properties(demo_cow PROPERTIES
        POSITION_INDEPENDENT_CODE FALSE
    )

    # 将 COW demo 链接到基础 demo 库
    target_link_libraries(demo PUBLIC demo_cow)
    # 将宏定义传递到基础 demo 库
    target_compile_definitions(demo PUBLIC COW_DEMO_ENABLED=1)
else()
    message(STATUS "COW Demo: DISABLED")
endif()

# Process Simple Demo
if(ENABLE_PROCESS_DEMO AND CCOS_ENABLE_DEMO)
    list(APPEND DEMO_ENABLED_LIST "PROCESS")
    message(STATUS "Process Simple Demo: ENABLED")

    add_library(demo_process STATIC process_simple/process_demo.c)

    target_include_directories(demo_process PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_BINARY_DIR}/include
    )

    target_compile_options(demo_process PUBLIC ${CCOS_FREESTANDING_FLAGS})
    target_compile_definitions(demo_process PUBLIC PROCESS_DEMO_ENABLED=1)

    # Process demo 依赖 process, klogs, list, bitmap
    target_link_libraries(demo_process PUBLIC process klogs list bitmap base)

    set_target_properties(demo_process PROPERTIES
        POSITION_INDEPENDENT_CODE FALSE
    )

    # 将 process demo 链接到基础 demo 库
    target_link_libraries(demo PUBLIC demo_process)
    # 将宏定义传递到基础 demo 库
    target_compile_definitions(demo PUBLIC PROCESS_DEMO_ENABLED=1)
else()
    message(STATUS "Process Simple Demo: DISABLED")
endif()

# Mock Syscall Demo
if(ENABLE_MOCK_SYSCALL_DEMO AND CCOS_ENABLE_DEMO)
    list(APPEND DEMO_ENABLED_LIST "MOOCK_SYSCALL")
    message(STATUS "Mock Syscall Demo: ENABLED")

    add_library(demo_mock_syscall STATIC mock_syscall/mock_syscall_demo.c)

    target_include_directories(demo_mock_syscall PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_BINARY_DIR}/include
    )

    target_compile_options(demo_mock_syscall PUBLIC ${CCOS_FREESTANDING_FLAGS})
    target_compile_definitions(demo_mock_syscall PUBLIC MOCK_SYSCALL_DEMO_ENABLED=1)

    # Mock syscall demo 依赖 syscall 模块
    target_link_libraries(demo_mock_syscall PUBLIC syscall klogs)

    set_target_properties(demo_mock_syscall PROPERTIES
        POSITION_INDEPENDENT_CODE FALSE
    )

    # 将 mock_syscall demo 链接到基础 demo 库
    target_link_libraries(demo PUBLIC demo_mock_syscall)
    # 将宏定义传递到基础 demo 库
    target_compile_definitions(demo PUBLIC MOCK_SYSCALL_DEMO_ENABLED=1)
else()
    message(STATUS "Mock Syscall Demo: DISABLED")
endif()

# Sched Demo
if(ENABLE_SCHED_DEMO AND CCOS_ENABLE_DEMO)
    list(APPEND DEMO_ENABLED_LIST "SCHED")
    message(STATUS "Sched Demo: ENABLED")

    add_library(demo_sched STATIC sched/sched_demo.c)

    target_include_directories(demo_sched PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_BINARY_DIR}/include
    )

    target_compile_options(demo_sched PUBLIC ${CCOS_FREESTANDING_FLAGS})
    target_compile_definitions(demo_sched PUBLIC SCHED_DEMO_ENABLED=1)

    # Sched demo 依赖 process, klogs, base
    target_link_libraries(demo_sched PUBLIC process klogs base)

    set_target_properties(demo_sched PROPERTIES
        POSITION_INDEPENDENT_CODE FALSE
    )

    # 将 sched demo 链接到基础 demo 库
    target_link_libraries(demo PUBLIC demo_sched)
    # 将宏定义传递到 basic demo 库
    target_compile_definitions(demo PUBLIC SCHED_DEMO_ENABLED=1)
else()
    message(STATUS "Sched Demo: DISABLED")
endif()

# User Mode Demo
if(ENABLE_USER_DEMO AND CCOS_ENABLE_DEMO)
    list(APPEND DEMO_ENABLED_LIST "USER")
    message(STATUS "User Mode Demo: ENABLED")

    add_library(demo_user STATIC user/user_demo.c)

    target_include_directories(demo_user PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_BINARY_DIR}/include
    )

    target_compile_options(demo_user PUBLIC ${CCOS_FREESTANDING_FLAGS})
    target_compile_definitions(demo_user PUBLIC USER_DEMO_ENABLED=1)

    # User demo 依赖 user, process, syscall, klogs, vmm, pframe, base
    target_link_libraries(demo_user PUBLIC user process syscall klogs kernel_vmm pframe base)

    set_target_properties(demo_user PROPERTIES
        POSITION_INDEPENDENT_CODE FALSE
    )

    # 将 user demo 链接到基础 demo 库
    target_link_libraries(demo PUBLIC demo_user)
    # 将宏定义传递到基础 demo 库
    target_compile_definitions(demo PUBLIC USER_DEMO_ENABLED=1)
else()
    message(STATUS "User Mode Demo: DISABLED")
endif()

# 根据 CCOS_ENABLE_DEMO 定义编译宏
# 只有在启用时才定义宏，这样 #ifdef CCOS_ENABLE_DEMO 才能正确工作
if(CCOS_ENABLE_DEMO)
    target_compile_definitions(demo PUBLIC CCOS_ENABLE_DEMO=1)
endif()
# 不定义任何值时，#ifdef CCOS_ENABLE_DEMO 将返回 false

# 打印启用的 demo 列表
list(LENGTH DEMO_ENABLED_LIST DEMO_COUNT)
if(DEMO_COUNT GREATER 0)
    message(STATUS "Enabled Demos: ${DEMO_ENABLED_LIST}")
else()
    message(STATUS "No individual demos enabled")
endif()

if(NOT CCOS_ENABLE_DEMO)
    message(STATUS "Demo functionality DISABLED at runtime (CCOS_ENABLE_DEMO=OFF)")
endif()
