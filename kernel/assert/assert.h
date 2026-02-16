#pragma once

/* Assert macro - condition expression must be true */
#define CCOS_ASSERT(EXPECTED_EXPR) \
    ccos_assert_impl(EXPECTED_EXPR, #EXPECTED_EXPR, __FILE__, __LINE__, __func__)

/* Backend function declarations */
void ccos_assert_impl(bool condition, const char* expr_str, const char* file, int line,
                      const char* func);

#ifdef NDEBUG
#    define CCOS_DEBUG_ASSERT(EXPECTED_EXPR) (void)(EXPECTED_EXPR)
#else
#    define CCOS_DEBUG_ASSERT(EXPECTED_EXPR) \
        ccos_assert_impl(EXPECTED_EXPR, #EXPECTED_EXPR, __FILE__, __LINE__, __func__)
#endif
