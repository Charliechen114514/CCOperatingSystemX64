# CCOS - Virtual Memory Management (VMM) CMakeLists.txt

cmake_minimum_required(VERSION 3.20)

# Collect all source files in the VMM directory
file(GLOB VMM_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

# Collect all header files
file(GLOB VMM_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# Create the VMM static library
add_library(kernel_vmm STATIC ${VMM_SOURCES} ${VMM_HEADERS})

# Set include directories
target_include_directories(kernel_vmm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kernel
)

# Apply freestanding flags (required for kernel code model)
target_compile_options(kernel_vmm PUBLIC ${CCOS_FREESTANDING_FLAGS})

# Apply frame pointer flags if enabled
if(CCOS_ENABLE_FRAME_POINTER)
    target_compile_options(kernel_vmm PUBLIC ${CCOS_FRAME_POINTER_FLAGS})
endif()

set_target_properties(kernel_vmm PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)

# Link required dependencies
target_link_libraries(kernel_vmm PUBLIC
    kernel_pframe
    kernel_interrupt
    kernel_base
    kernel_klogs
    kernel_assert
)


# Add debug symbols if debug build
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(kernel_vmm PRIVATE -g)
endif()

# Print configuration
message(STATUS "Configured VMM module")
