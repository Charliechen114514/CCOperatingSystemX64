# ============================================================================
# Assert 模块
# ============================================================================

add_library(assert STATIC
    assert.c
    assert_action_backend.c
)

target_include_directories(assert PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(assert PUBLIC ${CCOS_FREESTANDING_FLAGS})

set_target_properties(assert PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
