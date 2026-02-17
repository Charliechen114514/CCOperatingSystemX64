/**
 * @file ksnprintf.h
 * @brief Kernel snprintf implementation for string formatting
 *
 * A minimal snprintf implementation for kernel use.
 * Supports: %c, %s, %d/%i, %u, %x, %lx, %p
 * Width/flags: %Nx, %0Nx, %-Ns
 */

#pragma once

#include "defines/types.h"
#include "base/varargs.h"

/**
 * @brief Write formatted output to a string buffer
 *
 * @param buffer Buffer to write to
 * @param size Buffer size (including space for null terminator)
 * @param format Printf-style format string
 * @param ... Variable arguments
 * @return int Number of characters written (excluding null terminator), or negative on error
 */
int ksnprintf(char* buffer, size_t size, const char* format, ...);

/**
 * @brief Write formatted output to a string buffer with va_list
 *
 * @param buffer Buffer to write to
 * @param size Buffer size (including space for null terminator)
 * @param format Printf-style format string
 * @param args Variable arguments as va_list
 * @return int Number of characters written (excluding null terminator), or negative on error
 */
int kvsnprintf(char* buffer, size_t size, const char* format, va_list args);
