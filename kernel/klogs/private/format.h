/**
 * @file format.h
 * @brief Internal formatting utilities for klogs subsystem
 *
 * This module provides shared string formatting functionality
 * used by both kprintf and ksnprintf.
 */

#pragma once

#include "defines/types.h"
#include "base/varargs.h"

/**
 * @brief Format string to buffer (internal vsnprintf implementation)
 *
 * Supports: %c, %s, %d/%i, %u, %x, %lx, %p
 * Width/flags: %Nx, %0Nx, %-Ns (where N is width, - means left align)
 *
 * @param buffer Buffer to write to
 * @param size Buffer size (including space for null terminator)
 * @param format Printf-style format string
 * @param args Variable arguments
 * @return int Number of characters written (excluding null terminator)
 */
int klog_format_string(char* buffer, size_t size, const char* format, va_list args);
