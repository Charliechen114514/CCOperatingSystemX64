# ============================================================================
# Memory Management Module
# ============================================================================

# Add memory detection subdirectory
add_subdirectory(memory_detect)

# Add physical frame allocator subdirectory
add_subdirectory(pframe)

# Create the mm library as an interface library that aggregates memory components
add_library(mm INTERFACE)
# This is an interface library - no sources, just links the sub-libraries
target_link_libraries(mm INTERFACE
    memory_detect
    pframe
)

target_include_directories(mm INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../defines
    ${CMAKE_BINARY_DIR}/include
)
