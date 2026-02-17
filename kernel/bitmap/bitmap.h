/**
 * @file bitmap.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Basic bitmap (bit array) operations and utilities.
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 * This file provides a set of functions to manipulate bitmaps (arrays of bits).
 * Features include:
 * - Single bit operations (set, clear, flip, test)
 * - Range operations
 * - Bit scanning (find first/next set or zero bit)
 * - Bitmap logical operations (and, or, xor, andnot, complement)
 * - Utility functions (weight, empty/full, copy, compare)
 */

#pragma once
#include "defines/types.h"

/**
 * @struct bitmap
 * @brief Represents a bitmap (bit array) structure.
 *
 * Each bit in the bitmap can be individually manipulated. The bits are stored
 * in a contiguous memory buffer.
 */
typedef struct bitmap {
    byte_t* bits; /**< Pointer to the underlying bit storage buffer. */
    size_t nbits; /**< Total number of bits in the bitmap. */
} bitmap;

/* ===== Initialization ===== */

/**
 * @brief Initialize a bitmap structure.
 *
 * This function sets up the bitmap to use an existing buffer. It does not allocate memory.
 *
 * @param bm Pointer to the bitmap to initialize.
 * @param buffer Pointer to the memory buffer that stores the bits.
 * @param nbits Number of bits the bitmap should manage.
 */
void bitmap_init(struct bitmap* bm, void* buffer, size_t nbits);

/* ===== Single bit operations ===== */

/**
 * @brief Set a specific bit in the bitmap.
 *
 * @param bm Pointer to the bitmap.
 * @param bit Index of the bit to set (0-based).
 */
void bitmap_set(struct bitmap* bm, size_t bit);

/**
 * @brief Clear a specific bit in the bitmap.
 *
 * @param bm Pointer to the bitmap.
 * @param bit Index of the bit to clear (0-based).
 */
void bitmap_clear(struct bitmap* bm, size_t bit);

/**
 * @brief Test whether a specific bit is set.
 *
 * @param bm Pointer to the bitmap.
 * @param bit Index of the bit to test (0-based).
 * @return true if the bit is set, false otherwise.
 */
bool bitmap_test(const struct bitmap* bm, size_t bit);

/**
 * @brief Flip (invert) a specific bit in the bitmap.
 *
 * @param bm Pointer to the bitmap.
 * @param bit Index of the bit to flip (0-based).
 */
void bitmap_flip(struct bitmap* bm, size_t bit);

/* ===== Range operations ===== */

/**
 * @brief Set a range of bits in the bitmap.
 *
 * @param bm Pointer to the bitmap.
 * @param start Starting bit index (0-based).
 * @param count Number of consecutive bits to set.
 */
void bitmap_set_range(struct bitmap* bm, size_t start, size_t count);

/**
 * @brief Clear a range of bits in the bitmap.
 *
 * @param bm Pointer to the bitmap.
 * @param start Starting bit index (0-based).
 * @param count Number of consecutive bits to clear.
 */
void bitmap_clear_range(struct bitmap* bm, size_t start, size_t count);

/* ===== Bit scanning ===== */

/**
 * @brief Find the index of the first zero bit in the bitmap.
 *
 * @param bm Pointer to the bitmap.
 * @return Index of the first zero bit, or -1 if all bits are set.
 */
ssize_t bitmap_find_first_zero(const struct bitmap* bm);

/**
 * @brief Find the index of the first set bit in the bitmap.
 *
 * @param bm Pointer to the bitmap.
 * @return Index of the first set bit, or -1 if all bits are zero.
 */
ssize_t bitmap_find_first_set(const struct bitmap* bm);

/**
 * @brief Find the index of the next zero bit starting from a given position.
 *
 * @param bm Pointer to the bitmap.
 * @param start Starting index for the search (inclusive).
 * @return Index of the next zero bit, or -1 if none found.
 */
ssize_t bitmap_find_next_zero(const struct bitmap* bm, size_t start);

/**
 * @brief Find the index of the next set bit starting from a given position.
 *
 * @param bm Pointer to the bitmap.
 * @param start Starting index for the search (inclusive).
 * @return Index of the next set bit, or -1 if none found.
 */
ssize_t bitmap_find_next_set(const struct bitmap* bm, size_t start);

/* ===== Bitmap logical operations ===== */

/**
 * @brief Check whether two bitmaps are equal.
 *
 * @param a Pointer to the first bitmap.
 * @param b Pointer to the second bitmap.
 * @return true if both bitmaps have identical bits, false otherwise.
 */
bool bitmap_equal(const struct bitmap* a, const struct bitmap* b);

/**
 * @brief Copy the contents of one bitmap to another.
 *
 * @param dst Pointer to the destination bitmap.
 * @param src Pointer to the source bitmap.
 */
void bitmap_copy(struct bitmap* dst, const struct bitmap* src);

/**
 * @brief Perform bitwise AND between two bitmaps and store the result in a destination bitmap.
 *
 * @param dst Destination bitmap to store the result.
 * @param a First input bitmap.
 * @param b Second input bitmap.
 */
void bitmap_and(struct bitmap* dst, const struct bitmap* a, const struct bitmap* b);

/**
 * @brief Perform bitwise OR between two bitmaps and store the result in a destination bitmap.
 *
 * @param dst Destination bitmap to store the result.
 * @param a First input bitmap.
 * @param b Second input bitmap.
 */
void bitmap_or(struct bitmap* dst, const struct bitmap* a, const struct bitmap* b);

/**
 * @brief Perform bitwise XOR between two bitmaps and store the result in a destination bitmap.
 *
 * @param dst Destination bitmap to store the result.
 * @param a First input bitmap.
 * @param b Second input bitmap.
 */
void bitmap_xor(struct bitmap* dst, const struct bitmap* a, const struct bitmap* b);

/**
 * @brief Perform bitwise AND NOT (a & ~b) between two bitmaps.
 *
 * @param dst Destination bitmap to store the result.
 * @param a First input bitmap.
 * @param b Second input bitmap.
 */
void bitmap_andnot(struct bitmap* dst, const struct bitmap* a, const struct bitmap* b);

/**
 * @brief Compute the bitwise complement of a bitmap and store it in the destination bitmap.
 *
 * @param dst Destination bitmap to store the complement.
 * @param src Source bitmap.
 */
void bitmap_complement(struct bitmap* dst, const struct bitmap* src);

/* ===== Utility functions ===== */

/**
 * @brief Count the number of bits set to 1 in the bitmap.
 *
 * @param bm Pointer to the bitmap.
 * @return Number of bits set.
 */
size_t bitmap_weight(const struct bitmap* bm);

/**
 * @brief Check if all bits in the bitmap are set.
 *
 * @param bm Pointer to the bitmap.
 * @return true if all bits are 1, false otherwise.
 */
bool bitmap_full(const struct bitmap* bm);

/**
 * @brief Check if all bits in the bitmap are cleared.
 *
 * @param bm Pointer to the bitmap.
 * @return true if all bits are 0, false otherwise.
 */
bool bitmap_empty(const struct bitmap* bm);

/**
 * @brief Convert the bitmap into a string of '0' and '1' characters.
 *
 * @param buf Output buffer to store the string. Must be at least bm->nbits + 1 bytes.
 * @param bm Pointer to the bitmap.
 */
void bitmap_to_string(char* buf, const struct bitmap* bm);
