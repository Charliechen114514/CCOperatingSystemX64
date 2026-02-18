# COW Demo

add_library(cow_demo STATIC
    cow_demo.c
)

target_include_directories(cow_demo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

target_compile_options(cow_demo PUBLIC ${CCOS_FREESTANDING_FLAGS})

target_link_libraries(cow_demo PUBLIC
    demo
    kernel_vmm
    kernel_base
    kernel_klogs
    kernel_assert
)

set_target_properties(cow_demo PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
