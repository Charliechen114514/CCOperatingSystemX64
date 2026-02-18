# ============================================================================
# Kernel Heap Allocator
# ============================================================================

add_library(kernel_heap STATIC
    heap.c
)

target_include_directories(kernel_heap PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(kernel_heap PUBLIC ${CCOS_FREESTANDING_FLAGS})

if(CCOS_ENABLE_FRAME_POINTER)
    target_compile_options(kernel_heap PUBLIC ${CCOS_FRAME_POINTER_FLAGS})
endif()

target_link_libraries(kernel_heap PUBLIC
    kernel_vmm
    klogs
    base
    assert
)

set_target_properties(kernel_heap PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
