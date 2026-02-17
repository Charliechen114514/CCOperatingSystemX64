# ============================================================================
# Stacktrace 模块 - 栈回溯等调试功能
# ============================================================================

add_library(stacktrace STATIC
    stacktrace.c
)

target_include_directories(stacktrace PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(stacktrace PUBLIC ${CCOS_FREESTANDING_FLAGS})

set_target_properties(stacktrace PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
