# ============================================================================
# User Mode Support Module
# ============================================================================

# User mode C code
add_library(user OBJECT
    user.c
)

target_include_directories(user PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../defines
)

target_compile_options(user PUBLIC ${CCOS_FREESTANDING_FLAGS})

if(CCOS_ENABLE_FRAME_POINTER)
    target_compile_options(user PUBLIC ${CCOS_FRAME_POINTER_FLAGS})
endif()

set_target_properties(user PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)

# User mode assembly stubs - build as a separate custom target
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/user_enter_asm.o
    COMMAND ${NASM} -f elf64
        ${CMAKE_CURRENT_SOURCE_DIR}/user_enter.asm
        -o ${CMAKE_BINARY_DIR}/user_enter_asm.o
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/user_enter.asm
    COMMENT "Building user mode entry assembly stubs"
    VERBATIM
)

# Create a custom target that depends on the user library to ensure assembly is built
add_custom_target(user_asm ALL
    DEPENDS ${CMAKE_BINARY_DIR}/user_enter_asm.o
)

# Make the user library depend on the assembly
add_dependencies(user user_asm)
