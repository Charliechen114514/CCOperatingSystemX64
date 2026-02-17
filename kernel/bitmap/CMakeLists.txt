# ============================================================================
# Bitmap 位图模块
# ============================================================================

add_library(bitmap STATIC
    bitmap.c
)

target_include_directories(bitmap PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(bitmap PUBLIC ${CCOS_FREESTANDING_FLAGS})

# 链接依赖模块
target_link_libraries(bitmap PUBLIC
    assert
    base
    math
)

set_target_properties(bitmap PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
