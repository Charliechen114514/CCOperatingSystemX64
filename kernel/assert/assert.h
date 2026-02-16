#pragma once

/* Assert macro - condition expression must be true */
#define CCOS_ASSERT(EXPECTED_EXPR) \
    ccos_assert_impl(EXPECTED_EXPR, #EXPECTED_EXPR, __FILE__, __LINE__, __func__)

/* Backend function declarations */
void ccos_assert_impl(bool condition, const char* expr_str, const char* file, int line,
                      const char* func);

#ifdef NDEBUG
#    pragma message("Release Compile will not enable CCOS_DEBUG_ASSERT")
#    define CCOS_DEBUG_ASSERT(EXPECTED_EXPR) (void)(EXPECTED_EXPR)
#else
#    pragma message( \
        "Debug Compile will enable CCOS_DEBUG_ASSERT, which will be same as CCOS_ASSERT")
#    define CCOS_DEBUG_ASSERT(EXPECTED_EXPR) \
        ccos_assert_impl(EXPECTED_EXPR, #EXPECTED_EXPR, __FILE__, __LINE__, __func__)
#endif
