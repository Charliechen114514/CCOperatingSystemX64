# ============================================================================
# Memory Detection Module (E820)
# ============================================================================

add_library(memory_detect STATIC
    e820.c
    memory_state_helper.c
)

target_include_directories(memory_detect PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(memory_detect PUBLIC ${CCOS_FREESTANDING_FLAGS})

# Apply frame pointer flags if enabled
if(CCOS_ENABLE_FRAME_POINTER)
    target_compile_options(memory_detect PUBLIC ${CCOS_FRAME_POINTER_FLAGS})
endif()

# Link dependencies
target_link_libraries(memory_detect PUBLIC
    klogs
    base
)

set_target_properties(memory_detect PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
