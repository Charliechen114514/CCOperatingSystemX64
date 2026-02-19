# ============================================================================
# Math 数学模块
# ============================================================================

add_library(math STATIC
    math.c)

target_include_directories(math PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Apply freestanding flags (required for kernel code model)
target_compile_options(math PUBLIC ${CCOS_FREESTANDING_FLAGS})

# Apply frame pointer flags if enabled
if(CCOS_ENABLE_FRAME_POINTER)
    target_compile_options(math PUBLIC ${CCOS_FRAME_POINTER_FLAGS})
endif()

set_target_properties(math PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
