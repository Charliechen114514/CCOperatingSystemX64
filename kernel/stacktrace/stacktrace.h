/**
 * @file stacktrace.h
 * @brief Stack trace dump functionality for kernel debugging
 * @date 2026-02-17
 */

#pragma once

#include "defines/types.h"

/**
 * @brief Default maximum number of stack frames to dump
 */
#define DUMP_STACK_DEFAULT_FRAMES 16

/**
 * @brief Maximum allowed frames to prevent infinite loops
 */
#define DUMP_STACK_MAX_FRAMES 64

/**
 * @brief Dump the current stack trace
 *
 * Walks the stack frame chain using RBP and prints return addresses.
 *
 * @param max_frames Maximum number of frames to dump (0 for default)
 *
 * Example:
 *   void some_function() {
 *       dump_stack(16);  // Print up to 16 stack frames
 *   }
 */
void dump_stack(int max_frames);

/**
 * @brief Dump stack trace with default frame count
 */
void dump_stack_full(void);
