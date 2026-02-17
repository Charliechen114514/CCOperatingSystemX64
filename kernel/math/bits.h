/**
 * @file bits.h
 * @brief Bit manipulation and alignment utility functions
 *
 * This header provides a collection of macros and inline functions for
 * bit-level operations commonly used in kernel development, including
 * bit manipulation, power-of-2 calculations, memory alignment, and
 * integer division operations.
 */

/**
 * @brief Check if a number is an exact power of 2
 *
 * Determines whether the given value is a power of 2 using the classic
 * bit trick: powers of 2 have exactly one bit set, so n & (n-1) is zero.
 *
 * @param n The unsigned long value to test
 * @return true if n is a power of 2, false otherwise
 * @note Returns false for n = 0
 */
static inline bool is_power_of_2(unsigned long n) {
    return n != 0 && (n & (n - 1)) == 0;
}

/**
 * @brief Round a number up to the next power of 2
 *
 * Computes the smallest power of 2 that is greater than or equal to n.
 * Uses bit manipulation to set all bits below the most significant set bit,
 * then adds 1 to obtain the next power of 2.
 *
 * @param n The unsigned long value to round up
 * @return The smallest power of 2 >= n (returns 1 for n <= 1)
 *
 * @note For n = 0, returns 1 (not mathematically correct but useful for allocators)
 * @note This operation is commonly used for sizing hash tables, memory pools, etc.
 */
static inline unsigned long round_up_to_power_of_2(unsigned long n) {
    if (n <= 1)
        return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}

/**
 * @brief Round a number down to the previous power of 2
 *
 * Computes the largest power of 2 that is less than or equal to n.
 * Scans from the most significant bit to find the highest set bit.
 *
 * @param n The unsigned long value to round down
 * @return The largest power of 2 <= n (returns 0 for n = 0)
 *
 * @note If n is already a power of 2, returns n unchanged
 */
static inline unsigned long round_down_to_power_of_2(unsigned long n) {
    if (n == 0)
        return 0;
    unsigned long p = 1UL;
    for (int i = 63; i >= 0; i--) {
        if (n & (1UL << i)) {
            p = 1UL << i;
            break;
        }
    }
    return p;
}

/**
 * @brief Align a value up to the specified alignment boundary
 *
 * Rounds up the given value to the next multiple of alignment.
 * The alignment must be a power of 2 for correct operation.
 *
 * @param value The value to align
 * @param alignment The alignment boundary (must be power of 2)
 * @return The aligned value (smallest multiple of alignment >= value)
 *
 * @note If alignment is not a power of 2, results are undefined
 * @note Commonly used for memory allocation and page alignment
 */
static inline unsigned long align_up(unsigned long value, unsigned long alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief Align a value down to the specified alignment boundary
 *
 * Rounds down the given value to the previous multiple of alignment.
 * The alignment must be a power of 2 for correct operation.
 *
 * @param value The value to align
 * @param alignment The alignment boundary (must be power of 2)
 * @return The aligned value (largest multiple of alignment <= value)
 *
 * @note If alignment is not a power of 2, results are undefined
 * @note Commonly used for page boundary calculations
 */
static inline unsigned long align_down(unsigned long value, unsigned long alignment) {
    return value & ~(alignment - 1);
}

/**
 * @brief Check if a value is aligned to the specified boundary
 *
 * Determines whether the given value is exactly divisible by the alignment.
 * The alignment must be a power of 2 for correct operation.
 *
 * @param value The value to check
 * @param alignment The alignment boundary (must be power of 2)
 * @return true if value is aligned to the boundary, false otherwise
 *
 * @note If alignment is not a power of 2, results are undefined
 */
static inline bool is_aligned(unsigned long value, unsigned long alignment) {
    return (value & (alignment - 1)) == 0;
}

/**
 * @brief Divide two integers with rounding up (ceiling division)
 *
 * Computes the ceiling of n/d, returning the smallest integer >= n/d.
 * This is equivalent to ceil((double)n / (double)d) but uses integer arithmetic.
 *
 * @param n The dividend (numerator)
 * @param d The divisor (denominator, must be non-zero)
 * @return The quotient rounded up to the nearest integer
 *
 * @note Behavior is undefined if d = 0
 * @note Commonly used for calculating the number of pages/blocks needed
 */
static inline unsigned long div_round_up(unsigned long n, unsigned long d) {
    return (n + d - 1) / d;
}

/**
 * @brief Divide two integers with rounding down (floor division)
 *
 * Computes the floor of n/d, equivalent to standard integer division.
 * This is provided for API completeness and code clarity.
 *
 * @param n The dividend (numerator)
 * @param d The divisor (denominator, must be non-zero)
 * @return The quotient rounded down to the nearest integer
 *
 * @note Behavior is undefined if d = 0
 * @note Equivalent to the standard / operator
 */
static inline unsigned long div_round_down(unsigned long n, unsigned long d) {
    return n / d;
}

/**
 * @brief Divide two integers with rounding to nearest
 *
 * Computes n/d rounded to the nearest integer (with ties rounding down).
 * Uses the "add half of divisor" technique to achieve rounding.
 *
 * @param n The dividend (numerator)
 * @param d The divisor (denominator, must be non-zero)
 * @return The quotient rounded to the nearest integer
 *
 * @note Behavior is undefined if d = 0
 * @note For exact halves (e.g., 5/2), rounds toward positive infinity
 */
static inline unsigned long div_round_nearest(unsigned long n, unsigned long d) {
    return (n + d / 2) / d;
}
