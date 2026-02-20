/* ==============================================================================
 * CCOS - Atomic Operations Implementation
 * ==============================================================================
 */

#include "sync/atomic.h"

/* ==============================================================================
 * Basic Read/Write Operations
 * ==============================================================================
 */

int atomic_read(const atomic_t *a) {
    return __atomic_load_n(&a->counter, __ATOMIC_SEQ_CST);
}

void atomic_write(atomic_t *a, int i) {
    __atomic_store_n(&a->counter, i, __ATOMIC_SEQ_CST);
}

/* ==============================================================================
 * Arithmetic Operations
 * ==============================================================================
 */

void atomic_inc(atomic_t *a) {
    (void)__atomic_add_fetch(&a->counter, 1, __ATOMIC_SEQ_CST);
}

void atomic_dec(atomic_t *a) {
    (void)__atomic_sub_fetch(&a->counter, 1, __ATOMIC_SEQ_CST);
}

void atomic_add(atomic_t *a, int i) {
    (void)__atomic_add_fetch(&a->counter, i, __ATOMIC_SEQ_CST);
}

void atomic_sub(atomic_t *a, int i) {
    (void)__atomic_sub_fetch(&a->counter, i, __ATOMIC_SEQ_CST);
}

/* ==============================================================================
 * Arithmetic Operations with Return Value
 * ==============================================================================
 */

int atomic_inc_return(atomic_t *a) {
    return __atomic_add_fetch(&a->counter, 1, __ATOMIC_SEQ_CST);
}

int atomic_dec_return(atomic_t *a) {
    return __atomic_sub_fetch(&a->counter, 1, __ATOMIC_SEQ_CST);
}

int atomic_add_return(atomic_t *a, int i) {
    return __atomic_add_fetch(&a->counter, i, __ATOMIC_SEQ_CST);
}

int atomic_sub_return(atomic_t *a, int i) {
    return __atomic_sub_fetch(&a->counter, i, __ATOMIC_SEQ_CST);
}

/* ==============================================================================
 * Conditional Operations
 * ==============================================================================
 */

bool atomic_dec_and_test(atomic_t *a) {
    return __atomic_sub_fetch(&a->counter, 1, __ATOMIC_SEQ_CST) == 0;
}

bool atomic_inc_and_test(atomic_t *a) {
    return __atomic_add_fetch(&a->counter, 1, __ATOMIC_SEQ_CST) == 0;
}

bool atomic_sub_and_test(atomic_t *a, int i) {
    return __atomic_sub_fetch(&a->counter, i, __ATOMIC_SEQ_CST) == 0;
}

/* ==============================================================================
 * Compare-and-Swap Operations
 * ==============================================================================
 */

bool atomic_compare_and_swap(atomic_t *a, int old_val, int new_val) {
    return __atomic_compare_exchange_n(
        &a->counter,
        &old_val,
        new_val,
        false,  /* weak */
        __ATOMIC_SEQ_CST,
        __ATOMIC_SEQ_CST
    );
}

bool atomic_compare_exchange(atomic_t *a, int *old_val, int new_val) {
    return __atomic_compare_exchange_n(
        &a->counter,
        old_val,
        new_val,
        false,
        __ATOMIC_SEQ_CST,
        __ATOMIC_SEQ_CST
    );
}

/* ==============================================================================
 * Exchange Operations
 * ==============================================================================
 */

int atomic_xchg(atomic_t *a, int new_val) {
    return __atomic_exchange_n(&a->counter, new_val, __ATOMIC_SEQ_CST);
}

/* ==============================================================================
 * Fetch Operations (return old value)
 * ==============================================================================
 */

int atomic_fetch_inc(atomic_t *a) {
    return __atomic_fetch_add(&a->counter, 1, __ATOMIC_SEQ_CST);
}

int atomic_fetch_dec(atomic_t *a) {
    return __atomic_fetch_sub(&a->counter, 1, __ATOMIC_SEQ_CST);
}

int atomic_fetch_add(atomic_t *a, int i) {
    return __atomic_fetch_add(&a->counter, i, __ATOMIC_SEQ_CST);
}

int atomic_fetch_sub(atomic_t *a, int i) {
    return __atomic_fetch_sub(&a->counter, i, __ATOMIC_SEQ_CST);
}

/* ==============================================================================
 * Bitwise Operations
 * ==============================================================================
 */

void atomic_or(atomic_t *a, int mask) {
    (void)__atomic_or_fetch(&a->counter, mask, __ATOMIC_SEQ_CST);
}

void atomic_and(atomic_t *a, int mask) {
    (void)__atomic_and_fetch(&a->counter, mask, __ATOMIC_SEQ_CST);
}

void atomic_xor(atomic_t *a, int mask) {
    (void)__atomic_xor_fetch(&a->counter, mask, __ATOMIC_SEQ_CST);
}

void atomic_set_bit(atomic_t *a, int nr) {
    atomic_or(a, (1 << nr));
}

void atomic_clear_bit(atomic_t *a, int nr) {
    atomic_and(a, ~(1 << nr));
}

bool atomic_test_and_set_bit(atomic_t *a, int nr) {
    int old_val = atomic_read(a);
    int new_val = old_val | (1 << nr);
    return !atomic_compare_and_swap(a, old_val, new_val);
}

bool atomic_test_and_clear_bit(atomic_t *a, int nr) {
    int old_val = atomic_read(a);
    int new_val = old_val & ~(1 << nr);
    return !atomic_compare_and_swap(a, old_val, new_val);
}
