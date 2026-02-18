# ============================================================================
# Interrupt Subsystem
# ============================================================================

# Create an object library for interrupt subsystem
add_library(interrupt OBJECT
    idt.c
    interrupt.c
    tss.c
    gdt.c
    exception.c
)

target_include_directories(interrupt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../driver
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(interrupt PUBLIC ${CCOS_FREESTANDING_FLAGS})
if(CCOS_ENABLE_FRAME_POINTER)
    target_compile_options(interrupt PUBLIC ${CCOS_FRAME_POINTER_FLAGS})
endif()

set_target_properties(interrupt PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
