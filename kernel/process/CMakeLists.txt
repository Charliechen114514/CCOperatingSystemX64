# ============================================================================
# Process Management Module
# ============================================================================

# Process management C code
add_library(process OBJECT
    process.c
    thread.c
    sched.c
    sched_rr.c
    sched_prio.c
)

target_include_directories(process PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../defines
)

target_compile_options(process PUBLIC ${CCOS_FREESTANDING_FLAGS})

if(CCOS_ENABLE_FRAME_POINTER)
    target_compile_options(process PUBLIC ${CCOS_FRAME_POINTER_FLAGS})
endif()

set_target_properties(process PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
