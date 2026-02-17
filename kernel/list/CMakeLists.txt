# ============================================================================
# List 嵌入式链表模块
# ============================================================================

add_library(list STATIC
    list.c
)

target_include_directories(list PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(list PUBLIC ${CCOS_FREESTANDING_FLAGS})

# 链接依赖模块 (list 依赖 base 的 help_macros.h)
target_link_libraries(list PUBLIC
    base
)

set_target_properties(list PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
