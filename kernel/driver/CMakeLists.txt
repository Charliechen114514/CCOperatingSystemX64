# ============================================================================
# 驱动程序库
# ============================================================================

add_library(driver STATIC
    vga/vga.c
    vga/vga_helpers.c
    vga/vga_example.c
    vga/gui_helper/gui_helper.c
    serial/serial.c
    serial/serial_color.c
    serial/serial_intr.c
    keyboard/keyboard.c
    io/io.c
    pic/pic.c
    timer/timer.c
    rtc/rtc.c
)

target_include_directories(driver PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(driver PUBLIC ${CCOS_FREESTANDING_FLAGS})

target_link_libraries(driver PUBLIC base klogs)

set_target_properties(driver PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
