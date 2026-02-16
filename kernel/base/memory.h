/**
 * @file memory.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief yes the memory operations
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once
#include "defines/types.h"

/**
 * @brief Fill a block of memory with a byte value.
 *
 * Sets the first @p n bytes of the memory area pointed to by @p s
 * to the specified value (converted to unsigned char).
 *
 * @param s Pointer to the memory block to fill.
 * @param c Value to set (converted to unsigned char).
 * @param n Number of bytes to set.
 * @return Pointer to the memory block @p s.
 */
void* memset(void* s, int c, size_t n);

/**
 * @brief Copy memory from source to destination.
 *
 * Copies @p n bytes from memory area @p src to memory area @p dest.
 * The memory areas must not overlap. If they overlap, use memmove().
 *
 * @param dest Destination memory area.
 * @param src Source memory area.
 * @param n Number of bytes to copy.
 * @return Pointer to the destination memory area @p dest.
 */
void* memcpy(void* dest, const void* src, size_t n);

/**
 * @brief Copy memory with overlap support.
 *
 * Copies @p n bytes from memory area @p src to memory area @p dest.
 * The memory areas may overlap; copying is performed safely.
 *
 * @param dest Destination memory area.
 * @param src Source memory area.
 * @param n Number of bytes to copy.
 * @return Pointer to the destination memory area @p dest.
 */
void* memmove(void* dest, const void* src, size_t n);

/**
 * @brief Compare two memory areas.
 *
 * Compares the first @p n bytes of memory areas @p s1 and @p s2.
 *
 * @param s1 First memory area.
 * @param s2 Second memory area.
 * @param n Number of bytes to compare.
 * @return An integer less than, equal to, or greater than zero if
 *         the first differing byte in @p s1 is found to be less than,
 *         to match, or be greater than the corresponding byte in @p s2.
 */
int memcmp(const void* s1, const void* s2, size_t n);
