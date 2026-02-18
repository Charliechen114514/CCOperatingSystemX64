# ============================================================================
# System Call Framework
# ============================================================================

# System call assembly stubs
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/syscall_asm.o
    COMMAND ${NASM} -f elf64
        ${CMAKE_CURRENT_SOURCE_DIR}/syscall.asm
        -o ${CMAKE_BINARY_DIR}/syscall_asm.o
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/syscall.asm
    COMMENT "Building syscall assembly stubs"
    VERBATIM
)

# Create syscall object library
add_library(syscall OBJECT
    syscall.c
    syscall_table.c
)

target_include_directories(syscall PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../interrupt
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(syscall PUBLIC ${CCOS_FREESTANDING_FLAGS})
if(CCOS_ENABLE_FRAME_POINTER)
    target_compile_options(syscall PUBLIC ${CCOS_FRAME_POINTER_FLAGS})
endif()

set_target_properties(syscall PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
