#pragma once
#include "bitmap.h"
#include "defines/types.h"

/* Helpers are helpers :) */
static inline size_t bitCntToByteCnt(size_t nbits) {
    return (nbits + 7) >> 3;
}

static inline bool bit_index_valid(const struct bitmap* bm, size_t given_index) {
    return given_index < bm->nbits;
}

/**
 * @brief Generate a bit mask for a single bit within a byte.
 *
 * This function calculates a mask with only one bit set to 1, corresponding
 * to the position of a given bit index within a single byte. It is intended
 * for use in bitmaps or bit arrays where bits are stored in consecutive bytes.
 *
 * For example, if you have a bitmap like this (byte array):
 *
 *     bits[0] = 0b00000000;
 *
 * Calling byte_bitmask(3) will return:
 *
 *     1u << (3 % 8) = 1u << 3 = 0b00001000
 *
 * You can then use this mask to set, clear, or test the 3rd bit of bits[0]:
 *
 *     bits[0] |= byte_bitmask(3);   // set bit 3
 *     bits[0] &= ~byte_bitmask(3);  // clear bit 3
 *     bool is_set = bits[0] & byte_bitmask(3); // test bit 3
 *
 * @param bit The global bit index (0-based) you want a mask for. This
 *            index may exceed 7; only the position within the byte is
 *            used (bit % 8).
 *
 * @return A uint8_t value where only the target bit within a byte is set to 1,
 *         and all other bits are 0. The mapping is:
 *         - bit % 8 = 0 → 0b00000001
 *         - bit % 8 = 1 → 0b00000010
 *         - ...
 *         - bit % 8 = 7 → 0b10000000
 *
 * @note This function does NOT access the bitmap array itself. It only
 *       produces a mask for a single byte. To operate on a full bitmap,
 *       you typically combine it with:
 *
 *           byte_index = bit / 8;
 *           bits[byte_index] |= byte_bitmask(bit);   // set
 *           bits[byte_index] &= ~byte_bitmask(bit);  // clear
 *           bits[byte_index] ^= byte_bitmask(bit);   // flip
 *           bool is_set = bits[byte_index] & byte_bitmask(bit); // test
 *
 * @attention The shift is done with `1u` (unsigned) to avoid undefined behavior
 *            for bit positions >= 8 when promoted to int before shifting.
 */
static inline uint8_t byte_bitmask(size_t bit) {
    return (uint8_t)(1u << (bit % 8));
}

/**
 * @brief   that is——if we given the index of a bit, as then get the
 *          index of bytes, like: 3 -> 0; 9 -> 1;
 *
 * @param bit
 * @return size_t
 */
static inline size_t bit_byte(size_t bit) {
    return bit >> 3;
}