#pragma once
/* Backend function declarations */
void ccos_assert_impl(bool condition, const char* expr_str, const char* file, int line,
                      const char* func);

void panic(const char* expr_str, const char* file, int line, const char* func);

#define CCOS_IF_PANIC(EXPECTED_EXPR)                             \
    do {                                                         \
        if (EXPECTED_EXPR) {                                     \
            panic(#EXPECTED_EXPR, __FILE__, __LINE__, __func__); \
        }                                                        \
    } while (0)

/**
 * @brief Panic with message - prints error log then calls panic
 * Usage: CCOS_PANIC("Error message");
 */
#define CCOS_PANIC(msg)                             \
    do {                                             \
        klog_error("[PANIC] %s at %s:%d in %s\n",    \
                  (msg), __FILE__, __LINE__, __func__); \
        panic((msg), __FILE__, __LINE__, __func__);  \
    } while (0)

/* Assert macro - condition expression must be true */
#define CCOS_ASSERT(EXPECTED_EXPR) \
    ccos_assert_impl(EXPECTED_EXPR, #EXPECTED_EXPR, __FILE__, __LINE__, __func__)

#ifdef NDEBUG
#    define CCOS_DEBUG_ASSERT(EXPECTED_EXPR) (void)(EXPECTED_EXPR)
#else
#    define CCOS_DEBUG_ASSERT(EXPECTED_EXPR) \
        ccos_assert_impl(EXPECTED_EXPR, #EXPECTED_EXPR, __FILE__, __LINE__, __func__)
#endif
