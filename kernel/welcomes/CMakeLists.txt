# ============================================================================
# Welcomes 欢迎界面模块
# ============================================================================

add_library(welcomes STATIC
    welcome.c
    vga_welcomes.c
    serial_welcome.c
)

target_include_directories(welcomes PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(welcomes PUBLIC ${CCOS_FREESTANDING_FLAGS})

# 链接依赖模块
target_link_libraries(welcomes PUBLIC base klogs driver process)

set_target_properties(welcomes PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
