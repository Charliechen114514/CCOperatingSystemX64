# ============================================================================
# Physical Frame Allocator
# ============================================================================

add_library(pframe STATIC
    pframe.c
)

target_include_directories(pframe PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(pframe PUBLIC ${CCOS_FREESTANDING_FLAGS})

# Apply frame pointer flags if enabled
if(CCOS_ENABLE_FRAME_POINTER)
    target_compile_options(pframe PUBLIC ${CCOS_FRAME_POINTER_FLAGS})
endif()

# Link dependencies (direct dependencies, no mm to avoid circular dependency)
target_link_libraries(pframe PUBLIC
    bitmap
    memory_detect
    klogs
    base
    assert
)

set_target_properties(pframe PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
