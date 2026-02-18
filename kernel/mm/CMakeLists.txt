# ============================================================================
# Memory Management Module
# ============================================================================

# Create the mm library
add_library(mm OBJECT
    e820.c
    memory_state_helper.c
)

target_include_directories(mm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../defines
    ${CMAKE_BINARY_DIR}/include
)

# Apply freestanding compilation flags
target_compile_options(mm PUBLIC ${CCOS_FREESTANDING_FLAGS})

# Apply frame pointer flags if enabled
if(CCOS_ENABLE_FRAME_POINTER)
    target_compile_options(mm PUBLIC ${CCOS_FRAME_POINTER_FLAGS})
endif()

# Disable PIC for kernel code
set_target_properties(mm PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
