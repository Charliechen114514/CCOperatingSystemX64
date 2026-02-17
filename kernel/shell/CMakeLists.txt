# ============================================================================
# Shell Library
# ============================================================================

add_library(shell STATIC
    shell.c
    backends/serial_shell.c
)

target_include_directories(shell PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(shell PUBLIC ${CCOS_FREESTANDING_FLAGS})

target_link_libraries(shell PUBLIC base klogs driver)

set_target_properties(shell PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
