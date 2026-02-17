# ============================================================================
# Math 数学模块
# ============================================================================

add_library(math STATIC 
    math.c)

target_include_directories(math INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
