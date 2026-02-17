#include "bitmap.h"
#include "assert/assert.h"
#include "base/memory.h"
#include "bitmap_helper.h"
#include "math/math.h"

void bitmap_init(bitmap* bm, void* buffer, size_t nbits) {
    // Assert the bm, buffer should never NULL
    CCOS_DEBUG_ASSERT(bm && buffer);
    // And it should be above zero
    CCOS_DEBUG_ASSERT(nbits > 0);

    bm->nbits = nbits;
    bm->bits = (uint8_t*)buffer;

    /* clear these */
    memset(buffer, 0, bitCntToByteCnt(nbits));
}

void bitmap_set(bitmap* bm, size_t bit) {
    CCOS_DEBUG_ASSERT(bm);
    CCOS_ASSERT(bit_index_valid(bm, bit));
    bm->bits[bit_byte(bit)] |= byte_bitmask(bit);
}

void bitmap_clear(bitmap* bm, size_t bit) {
    CCOS_DEBUG_ASSERT(bm);
    CCOS_ASSERT(bit_index_valid(bm, bit));
    bm->bits[bit_byte(bit)] &= ~byte_bitmask(bit);
}

bool bitmap_test(const bitmap* bm, size_t bit) {
    CCOS_ASSERT(bit_index_valid(bm, bit));
    return bm->bits[bit_byte(bit)] & byte_bitmask(bit);
}

void bitmap_flip(bitmap* bm, size_t bit) {
    CCOS_ASSERT(bit_index_valid(bm, bit));
    bm->bits[bit_byte(bit)] ^= byte_bitmask(bit);
}

// Simple range operations - set/clear bits one by one
// This is less efficient but simpler and more reliable
void bitmap_set_range(bitmap* bm, size_t start, size_t count) {
    CCOS_DEBUG_ASSERT(bm && count > 0);

    size_t end = min(start + count, bm->nbits);
    for (size_t i = start; i < end; i++) {
        bitmap_set(bm, i);
    }
}

void bitmap_clear_range(bitmap* bm, size_t start, size_t count) {
    CCOS_DEBUG_ASSERT(bm && count > 0);

    size_t end = min(start + count, bm->nbits);
    for (size_t i = start; i < end; i++) {
        bitmap_clear(bm, i);
    }
}

ssize_t bitmap_find_first_zero(const bitmap* bm) {
    return bitmap_find_next_zero(bm, 0);
}

ssize_t bitmap_find_first_set(const bitmap* bm) {
    return bitmap_find_next_set(bm, 0);
}

ssize_t bitmap_find_next_zero(const bitmap* bm, size_t start) {
    for (size_t i = start; i < bm->nbits; i++)
        if (!bitmap_test(bm, i))
            return (ssize_t)i;
    return -1;
}

ssize_t bitmap_find_next_set(const bitmap* bm, size_t start) {
    for (size_t i = start; i < bm->nbits; i++)
        if (bitmap_test(bm, i))
            return (ssize_t)i;
    return -1;
}

bool bitmap_equal(const bitmap* a, const bitmap* b) {
    if (a->nbits != b->nbits)
        return false;

    size_t bytes = bitCntToByteCnt(a->nbits);
    for (size_t i = 0; i < bytes; i++)
        if (a->bits[i] != b->bits[i])
            return false;
    return true;
}

void bitmap_copy(bitmap* dst, const bitmap* src) {
    size_t bytes = bitCntToByteCnt(src->nbits);
    memcpy(dst->bits, src->bits, bytes);
}

void bitmap_and(bitmap* dst, const bitmap* a, const bitmap* b) {
    size_t bytes = bitCntToByteCnt(a->nbits);
    for (size_t i = 0; i < bytes; i++)
        dst->bits[i] = a->bits[i] & b->bits[i];
}

void bitmap_or(bitmap* dst, const bitmap* a, const bitmap* b) {
    size_t bytes = bitCntToByteCnt(a->nbits);
    for (size_t i = 0; i < bytes; i++)
        dst->bits[i] = a->bits[i] | b->bits[i];
}

void bitmap_xor(bitmap* dst, const bitmap* a, const bitmap* b) {
    size_t bytes = bitCntToByteCnt(a->nbits);
    for (size_t i = 0; i < bytes; i++)
        dst->bits[i] = a->bits[i] ^ b->bits[i];
}

void bitmap_andnot(bitmap* dst, const bitmap* a, const bitmap* b) {
    size_t bytes = bitCntToByteCnt(a->nbits);
    for (size_t i = 0; i < bytes; i++)
        dst->bits[i] = a->bits[i] & ~b->bits[i];
}

void bitmap_complement(bitmap* dst, const bitmap* src) {
    size_t bytes = bitCntToByteCnt(src->nbits);
    for (size_t i = 0; i < bytes; i++)
        dst->bits[i] = ~src->bits[i];
}

void bitmap_to_string(char* buf, const bitmap* bm) {
    for (size_t i = 0; i < bm->nbits; i++)
        buf[i] = bitmap_test(bm, i) ? '1' : '0';
    buf[bm->nbits] = '\0';
}

size_t bitmap_weight(const bitmap* bm) {
    size_t count = 0;
    size_t bytes = bitCntToByteCnt(bm->nbits);

    for (size_t i = 0; i < bytes; i++) {
        uint8_t byte = bm->bits[i];
        // Count bits in each byte using Brian Kernighan's algorithm
        while (byte) {
            byte &= byte - 1;
            count++;
        }
    }
    return count;
}

bool bitmap_full(const bitmap* bm) {
    return bitmap_weight(bm) == bm->nbits;
}

bool bitmap_empty(const bitmap* bm) {
    return bitmap_weight(bm) == 0;
}