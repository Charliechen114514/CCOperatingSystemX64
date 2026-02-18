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
