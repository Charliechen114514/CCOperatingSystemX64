# ============================================================================
# Demo Programs - Optional demonstration code for kernel subsystems
# ============================================================================

# 检查全局 demo 开关（如果未定义，默认为 ON）
if(NOT DEFINED CCOS_ENABLE_DEMO)
    set(CCOS_ENABLE_DEMO ON CACHE BOOL "Enable kernel demo programs" FORCE)
endif()

# RTC Demo (default OFF for safety)
option(ENABLE_RTC_DEMO "Enable RTC driver demo" OFF)

# Demo sources list (always include all_demo.c as controller)
set(DEMO_SOURCES all_demo.c)

# 只有在全局 demo 开启时才构建 demo 库
if(CCOS_ENABLE_DEMO)
    # 检查各个 demo 开关并添加相应源文件
    if(ENABLE_RTC_DEMO)
        list(APPEND DEMO_SOURCES rtc/rtc_demo.c)
        set(RTC_DEMO_ENABLED TRUE)
        message(STATUS "RTC Demo: ENABLED")
    else()
        message(STATUS "RTC Demo: DISABLED")
    endif()

    # 创建 demo 库
    add_library(demo_rtc STATIC ${DEMO_SOURCES})

    target_include_directories(demo_rtc PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_BINARY_DIR}/include
    )

    target_compile_options(demo_rtc PUBLIC ${CCOS_FREESTANDING_FLAGS})

    target_link_libraries(demo_rtc PUBLIC driver klogs)

    set_target_properties(demo_rtc PROPERTIES
        POSITION_INDEPENDENT_CODE FALSE
    )

    # 根据启用的 demos 定义相应的宏
    if(RTC_DEMO_ENABLED)
        target_compile_definitions(demo_rtc PRIVATE RTC_DEMO_ENABLED=1)
    endif()

else()
    # 创建一个空接口库，避免链接错误
    add_library(demo_rtc INTERFACE)
    message(STATUS "All Demos: DISABLED (CCOS_ENABLE_DEMO=OFF)")
endif()
