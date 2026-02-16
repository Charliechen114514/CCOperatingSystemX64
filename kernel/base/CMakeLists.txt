# ============================================================================
# Base 基础库模块
# ============================================================================

add_library(base STATIC
    string.c
    memory.c
    strhelpers.c
)

target_include_directories(base PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(base PUBLIC ${CCOS_FREESTANDING_FLAGS})

# 链接 assert 模块 (base 依赖 assert)
target_link_libraries(base PUBLIC assert)

set_target_properties(base PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
