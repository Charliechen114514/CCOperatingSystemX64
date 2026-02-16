# ============================================================================
# 驱动程序库
# ============================================================================

add_library(driver STATIC
    vga/vga.c
    vga/vga_helpers.c
    vga/vga_example.c
    vga/gui_helper/gui_helper.c
)

target_include_directories(driver PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(driver PUBLIC ${CCOS_FREESTANDING_FLAGS})

set_target_properties(driver PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
