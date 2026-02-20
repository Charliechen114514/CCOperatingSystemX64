/* ==============================================================================
 * CCOS - Atomic Operations
 * ==============================================================================
 * Provides atomic operations for thread-safe access to shared variables.
 * Uses GCC/Clang built-in atomic functions for portable implementation.
 * ==============================================================================
 */

#pragma once

/* ==============================================================================
 * Atomic Type
 * ==============================================================================
 */

/**
 * @brief Atomic integer type
 *
 * Provides atomic access to an integer counter.
 * Uses volatile to ensure compiler does not optimize away accesses.
 */
typedef struct {
    volatile int counter;
} atomic_t;

/* ==============================================================================
 * Initialization Macros
 * ==============================================================================
 */

/**
 * @brief Static initializer for atomic_t
 */
#define ATOMIC_INIT(i) {.counter = (i)}

/**
 * @brief Initialize an atomic variable at runtime
 */
#define ATOMIC_INITIALIZER(i) ((atomic_t)ATOMIC_INIT(i))

/* ==============================================================================
 * Basic Read/Write Operations
 * ==============================================================================
 */

int atomic_read(const atomic_t* a);
void atomic_write(atomic_t* a, int i);

/* ==============================================================================
 * Arithmetic Operations
 * ==============================================================================
 */

void atomic_inc(atomic_t* a);
void atomic_dec(atomic_t* a);
void atomic_add(atomic_t* a, int i);
void atomic_sub(atomic_t* a, int i);

/* ==============================================================================
 * Arithmetic Operations with Return Value
 * ==============================================================================
 */

int atomic_inc_return(atomic_t* a);
int atomic_dec_return(atomic_t* a);
int atomic_add_return(atomic_t* a, int i);
int atomic_sub_return(atomic_t* a, int i);

/* ==============================================================================
 * Conditional Operations
 * ==============================================================================
 */

bool atomic_dec_and_test(atomic_t* a);
bool atomic_inc_and_test(atomic_t* a);
bool atomic_sub_and_test(atomic_t* a, int i);

/* ==============================================================================
 * Compare-and-Swap Operations
 * ==============================================================================
 */

bool atomic_compare_and_swap(atomic_t* a, int old_val, int new_val);
bool atomic_compare_exchange(atomic_t* a, int* old_val, int new_val);

/* ==============================================================================
 * Exchange Operations
 * ==============================================================================
 */

int atomic_xchg(atomic_t* a, int new_val);

/* ==============================================================================
 * Fetch Operations (return old value)
 * ==============================================================================
 */

int atomic_fetch_inc(atomic_t* a);
int atomic_fetch_dec(atomic_t* a);
int atomic_fetch_add(atomic_t* a, int i);
int atomic_fetch_sub(atomic_t* a, int i);

/* ==============================================================================
 * Bitwise Operations
 * ==============================================================================
 */

void atomic_or(atomic_t* a, int mask);
void atomic_and(atomic_t* a, int mask);
void atomic_xor(atomic_t* a, int mask);
void atomic_set_bit(atomic_t* a, int nr);
void atomic_clear_bit(atomic_t* a, int nr);
bool atomic_test_and_set_bit(atomic_t* a, int nr);
bool atomic_test_and_clear_bit(atomic_t* a, int nr);

/* ==============================================================================
 * Memory Barriers (must be inline)
 * ==============================================================================
 */

/**
 * @brief Compiler barrier - prevents compiler reordering
 */
#define barrier() __asm__ __volatile__("" ::: "memory")

/**
 * @brief Full memory barrier - ensures all memory operations are completed
 */
static inline void mb(void) {
    __asm__ __volatile__("mfence" ::: "memory");
}

/**
 * @brief Read memory barrier - ensures reads are completed
 */
static inline void rmb(void) {
    __asm__ __volatile__("lfence" ::: "memory");
}

/**
 * @brief Write memory barrier - ensures writes are completed
 */
static inline void wmb(void) {
    __asm__ __volatile__("sfence" ::: "memory");
}

/**
 * @brief Compiler barrier for SMP systems
 */
#define smp_mb() mb()
#define smp_rmb() rmb()
#define smp_wmb() wmb()
