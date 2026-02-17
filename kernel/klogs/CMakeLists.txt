# ============================================================================
# klogs - Kernel logging module with backend support
# ============================================================================

add_library(klogs OBJECT
    kprintf.c
    kprintf_backends.c
    backends/serial_backends.c
    ksnprintf.c
    private/format.c
)

target_include_directories(klogs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_options(klogs PUBLIC ${CCOS_FREESTANDING_FLAGS})

target_link_libraries(klogs PUBLIC base)

set_target_properties(klogs PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
