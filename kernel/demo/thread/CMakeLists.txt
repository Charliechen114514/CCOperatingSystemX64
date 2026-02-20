# Thread Demo

option(ENABLE_THREAD_DEMO "Enable Thread Creation demo" OFF)

if(ENABLE_THREAD_DEMO)
    add_library(demo_thread OBJECT
        thread_demo.c
    )

    target_include_directories(demo_thread PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
    )

    target_compile_options(demo_thread PUBLIC ${CCOS_FREESTANDING_FLAGS})

    if(CCOS_ENABLE_FRAME_POINTER)
        target_compile_options(demo_thread PUBLIC ${CCOS_FRAME_POINTER_FLAGS})
    endif()

    set_target_properties(demo_thread PROPERTIES
        POSITION_INDEPENDENT_CODE FALSE
    )
endif()
