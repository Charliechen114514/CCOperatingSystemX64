/* ==============================================================================
 * User Mode Demo Program - uname_test
 * ==============================================================================
 *
 * External declarations for the compiled user mode program.
 *
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start address of the uname_test user program
 */
extern const uint8_t _binary_user_programs_demo_uname_test_start[];

/**
 * @brief End address of the uname_test user program
 */
extern const uint8_t _binary_user_programs_demo_uname_test_end[];

/**
 * @brief Size of the uname_test user program
 */
extern const uint8_t _binary_user_programs_demo_uname_test_size[];

/**
 * @brief Get the uname_test program start address
 */
static inline const uint8_t* uname_test_program_start(void) {
    return _binary_user_programs_demo_uname_test_start;
}

/**
 * @brief Get the uname_test program size
 */
static inline size_t uname_test_program_size(void) {
    return (size_t)_binary_user_programs_demo_uname_test_size;
}

#ifdef __cplusplus
}
#endif
