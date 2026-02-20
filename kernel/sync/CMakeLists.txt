# ============================================================================
# CCOS - Synchronization Primitives Module
# ============================================================================

# Create an object library for sync subsystem
add_library(sync OBJECT
    atomic.c
    spinlock.c
    rwlock.c
    mutex.c
    semaphore.c
    condvar.c
    waitqueue.c
)

target_include_directories(sync PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}/include
)

target_compile_options(sync PUBLIC ${CCOS_FREESTANDING_FLAGS})
if(CCOS_ENABLE_FRAME_POINTER)
    target_compile_options(sync PUBLIC ${CCOS_FRAME_POINTER_FLAGS})
endif()

set_target_properties(sync PROPERTIES
    POSITION_INDEPENDENT_CODE FALSE
)
