/* ==============================================================================
 * CCOS User Library - Standard I/O
 * ==============================================================================
 *
 * Standard input/output interface for user programs.
 *
 * ==============================================================================
 */

#pragma once

#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Standard File Descriptors
 * ============================================================================ */

#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

/* ============================================================================
 * EOF Constant
 * ============================================================================ */

#define EOF     (-1)

/* ============================================================================
 * Standard I/O Functions
 * ============================================================================ */

/**
 * @brief Write formatted output to stdout
 * @param format Format string
 * @param ... Variable arguments
 * @return Number of characters printed, or negative on error
 */
int printf(const char* format, ...);

/**
 * @brief Write a string to stdout
 * @param s Null-terminated string
 * @return Number of characters written, or negative on error
 */
int puts(const char* s);

/**
 * @brief Write a string to a file descriptor
 * @param fd File descriptor
 * @param s Null-terminated string
 * @return Number of characters written, or negative on error
 */
int fputs(int fd, const char* s);

/**
 * @brief Write a single character to stdout
 * @param c Character to write
 * @return The character written, or EOF on error
 */
int putchar(int c);

/**
 * @brief Write a single character to a file descriptor
 * @param fd File descriptor
 * @param c Character to write
 * @return The character written, or EOF on error
 */
int fputc(int fd, int c);

/**
 * @brief Write formatted output to a file descriptor
 * @param fd File descriptor
 * @param format Format string
 * @param ... Variable arguments
 * @return Number of characters printed, or negative on error
 */
int fprintf(int fd, const char* format, ...);

#ifdef __cplusplus
}
#endif
