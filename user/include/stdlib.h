/* ==============================================================================
 * CCOS User Library - Standard Library Functions
 * ==============================================================================
 *
 * Memory allocation and utility functions.
 *
 * ==============================================================================
 */

#pragma once

#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Memory Allocation
 * ============================================================================ */

/**
 * @brief Allocate memory from heap
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void* malloc(size_t size);

/**
 * @brief Free previously allocated memory
 * @param ptr Pointer to memory to free (NULL is safe)
 */
void free(void* ptr);

/**
 * @brief Reallocate memory
 * @param ptr Pointer to previously allocated memory (NULL for new allocation)
 * @param size New size in bytes
 * @return Pointer to reallocated memory, or NULL on failure
 */
void* realloc(void* ptr, size_t size);

/**
 * @brief Allocate and zero-initialize memory
 * @param nmemb Number of elements
 * @param size Size of each element
 * @return Pointer to allocated memory, or NULL on failure
 */
void* calloc(size_t nmemb, size_t size);

/* ============================================================================
 * Process Control
 * ============================================================================ */

/**
 * @brief Abort the program abnormally
 * @note This function never returns
 */
void abort(void) __attribute__((noreturn));

/**
 * @brief Exit the program normally
 * @param status Exit code
 * @note This function never returns
 */
void exit(int status) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif
