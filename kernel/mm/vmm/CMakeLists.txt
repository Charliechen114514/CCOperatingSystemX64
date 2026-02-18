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

# Link required dependencies
target_link_libraries(kernel_vmm PUBLIC
    kernel_pframe
    kernel_interrupt
    kernel_base
    kernel_klogs
    kernel_assert
)

# Set compiler flags
target_compile_options(kernel_vmm PRIVATE
    -Wall
    -Wextra
    -O2
    -ffreestanding
    -mcmodel=large
    -mno-red-zone
    -mno-mmx
    -mno-sse
    -mno-sse2
)

# Add debug symbols if debug build
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(kernel_vmm PRIVATE -g)
endif()

# Print configuration
message(STATUS "Configured VMM module")
