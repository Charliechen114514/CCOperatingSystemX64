/**
 * @file test_string.c
 * @brief Comprehensive tests for kernel string utility functions
 */

// For hosted test environment, we need size_t before including string.h
// The kernel defines size_t only for freestanding environment
#include <stddef.h>
#include <stdint.h>

// Forward declare string functions to avoid including the full header
// in hosted environment where size_t comes from stddef.h
extern size_t strlen(const char* s);
extern size_t strnlen(const char* s, size_t maxlen);
extern char* strcpy(char* dest, const char* src);
extern char* strncpy(char* dest, const char* src, size_t n);
extern int strcmp(const char* s1, const char* s2);
extern int strncmp(const char* s1, const char* s2, size_t n);
extern int strcasecmp(const char* s1, const char* s2);
extern int strncasecmp(const char* s1, const char* s2, size_t n);
extern char tolower(char c);
extern char* strchr(const char* s, int c);
extern char* strrchr(const char* s, int c);
extern char* strstr(const char* haystack, const char* needle);
extern char* strpbrk(const char* s, const char* accept);
extern size_t strspn(const char* s, const char* accept);
extern size_t strcspn(const char* s, const char* reject);
extern char* strtok_r(char* str, const char* delim, char** saveptr);
extern char* strtok(char* str, const char* delim);

// Define global test counters
int g_test_passed = 0;
int g_test_failed = 0;

#include "host_support.h"

// ============================================================================
// strlen Tests
// ============================================================================

static void test_strlen(void) {
    TEST_INFO("Testing strlen...");

    // Basic cases
    TEST_ASSERT_EQ(strlen(""), (size_t)0, "strlen: empty string");
    TEST_ASSERT_EQ(strlen("a"), (size_t)1, "strlen: single character");
    TEST_ASSERT_EQ(strlen("hello"), (size_t)5, "strlen: normal string");
    TEST_ASSERT_EQ(strlen("hello world"), (size_t)11, "strlen: string with space");

    // Longer strings
    TEST_ASSERT_EQ(strlen("abcdefghijklmnopqrstuvwxyz"), (size_t)26,
                   "strlen: alphabet string");

    // String with special characters
    TEST_ASSERT_EQ(strlen("1234567890!@#$%^&*()"), (size_t)20,
                   "strlen: special characters");

    TEST_PASS("strlen: all tests passed");
}

// ============================================================================
// strnlen Tests
// ============================================================================

static void test_strnlen(void) {
    TEST_INFO("Testing strnlen...");

    // Maxlen larger than actual length
    TEST_ASSERT_EQ(strnlen("", 100), (size_t)0, "strnlen: empty string, large maxlen");
    TEST_ASSERT_EQ(strnlen("hello", 100), (size_t)5, "strnlen: normal string, large maxlen");

    // Maxlen smaller than actual length
    TEST_ASSERT_EQ(strnlen("hello", 3), (size_t)3, "strnlen: maxlen < length");
    TEST_ASSERT_EQ(strnlen("hello world", 5), (size_t)5, "strnlen: maxlen truncates");

    // Maxlen equal to length
    TEST_ASSERT_EQ(strnlen("hello", 5), (size_t)5, "strnlen: maxlen == length");

    // Edge case: maxlen = 0
    TEST_ASSERT_EQ(strnlen("hello", 0), (size_t)0, "strnlen: maxlen = 0");

    TEST_PASS("strnlen: all tests passed");
}

// ============================================================================
// strcpy Tests
// ============================================================================

static void test_strcpy(void) {
    TEST_INFO("Testing strcpy...");

    char buffer[100];

    // Basic copy
    strcpy(buffer, "hello");
    TEST_ASSERT_STR_EQ(buffer, "hello", "strcpy: basic copy");

    // Empty string copy
    strcpy(buffer, "");
    TEST_ASSERT_EQ(buffer[0], '\0', "strcpy: empty string");

    // String with spaces
    strcpy(buffer, "hello world");
    TEST_ASSERT_STR_EQ(buffer, "hello world", "strcpy: string with spaces");

    // Single character
    strcpy(buffer, "a");
    TEST_ASSERT_STR_EQ(buffer, "a", "strcpy: single character");

    // Return value check
    char* result = strcpy(buffer, "test");
    TEST_ASSERT_EQ(result, buffer, "strcpy: returns dest pointer");

    TEST_PASS("strcpy: all tests passed");
}

// ============================================================================
// strncpy Tests
// ============================================================================

static void test_strncpy(void) {
    TEST_INFO("Testing strncpy...");

    char buffer[100];

    // Copy less than source length
    strncpy(buffer, "hello", 3);
    buffer[3] = '\0'; // Manually terminate for testing
    TEST_ASSERT_STR_EQ(buffer, "hel", "strncpy: partial copy");

    // Copy exactly source length
    strncpy(buffer, "hello", 5);
    buffer[5] = '\0';
    TEST_ASSERT_STR_EQ(buffer, "hello", "strncpy: exact length");

    // Copy more than source length
    strncpy(buffer, "hi", 10);
    // Check that first chars are correct
    TEST_ASSERT_EQ(buffer[0], 'h', "strncpy: n > len, first char");
    TEST_ASSERT_EQ(buffer[1], 'i', "strncpy: n > len, second char");

    // Return value check
    char* result = strncpy(buffer, "test", 10);
    TEST_ASSERT_EQ(result, buffer, "strncpy: returns dest pointer");

    TEST_PASS("strncpy: all tests passed");
}

// ============================================================================
// strcmp Tests
// ============================================================================

static void test_strcmp(void) {
    TEST_INFO("Testing strcmp...");

    // Equal strings
    TEST_ASSERT_EQ(strcmp("", ""), 0, "strcmp: empty strings equal");
    TEST_ASSERT_EQ(strcmp("a", "a"), 0, "strcmp: single char equal");
    TEST_ASSERT_EQ(strcmp("hello", "hello"), 0, "strcmp: same strings equal");

    // s1 < s2
    TEST_ASSERT_TRUE(strcmp("a", "b") < 0, "strcmp: a < b");
    TEST_ASSERT_TRUE(strcmp("hello", "world") < 0, "strcmp: hello < world");
    TEST_ASSERT_TRUE(strcmp("abc", "abcd") < 0, "strcmp: prefix less than longer");

    // s1 > s2
    TEST_ASSERT_TRUE(strcmp("b", "a") > 0, "strcmp: b > a");
    TEST_ASSERT_TRUE(strcmp("world", "hello") > 0, "strcmp: world > hello");
    TEST_ASSERT_TRUE(strcmp("abcd", "abc") > 0, "strcmp: longer greater than prefix");

    // Case sensitivity
    TEST_ASSERT_TRUE(strcmp("Hello", "hello") != 0, "strcmp: case sensitive");

    TEST_PASS("strcmp: all tests passed");
}

// ============================================================================
// strncmp Tests
// ============================================================================

static void test_strncmp(void) {
    TEST_INFO("Testing strncmp...");

    // Equal within n
    TEST_ASSERT_EQ(strncmp("hello", "hello", 5), 0, "strncmp: equal strings");
    TEST_ASSERT_EQ(strncmp("hello", "hello world", 5), 0, "strncmp: prefix equal");
    TEST_ASSERT_EQ(strncmp("abc", "abcdef", 3), 0, "strncmp: first n equal");

    // Different within n
    TEST_ASSERT_TRUE(strncmp("abc", "abd", 3) < 0, "strncmp: differ at n");
    TEST_ASSERT_TRUE(strncmp("hello", "hallo", 3) > 0, "strncmp: differ early (e > a)");

    // n = 0 (always equal)
    TEST_ASSERT_EQ(strncmp("abc", "def", 0), 0, "strncmp: n=0 always equal");

    // n less than difference point
    TEST_ASSERT_EQ(strncmp("abc", "abx", 2), 0, "strncmp: equal within n");
    TEST_ASSERT_TRUE(strncmp("aaa", "abc", 2) < 0, "strncmp: differ at second char (a < b)");

    TEST_PASS("strncmp: all tests passed");
}

// ============================================================================
// strcasecmp Tests
// ============================================================================

static void test_strcasecmp(void) {
    TEST_INFO("Testing strcasecmp...");

    // Case insensitive equality
    TEST_ASSERT_EQ(strcasecmp("", ""), 0, "strcasecmp: empty strings");
    TEST_ASSERT_EQ(strcasecmp("hello", "HELLO"), 0, "strcasecmp: different case equal");
    TEST_ASSERT_EQ(strcasecmp("HeLLo WoRLd", "hello world"), 0,
                   "strcasecmp: mixed case equal");

    // Case insensitive comparison
    TEST_ASSERT_TRUE(strcasecmp("abc", "ABD") < 0, "strcasecmp: a < d (case insensitive)");
    TEST_ASSERT_TRUE(strcasecmp("ABC", "aaa") > 0, "strcasecmp: c > a (case insensitive)");

    // Single character
    TEST_ASSERT_EQ(strcasecmp("a", "A"), 0, "strcasecmp: single char");
    TEST_ASSERT_TRUE(strcasecmp("a", "b") < 0, "strcasecmp: single char diff");

    // Mixed content
    TEST_ASSERT_EQ(strcasecmp("Hello123", "hello123"), 0,
                   "strcasecmp: alphanumeric mixed case");

    TEST_PASS("strcasecmp: all tests passed");
}

// ============================================================================
// strncasecmp Tests
// ============================================================================

static void test_strncasecmp(void) {
    TEST_INFO("Testing strncasecmp...");

    // Case insensitive within n
    TEST_ASSERT_EQ(strncasecmp("Hello", "HELLO", 5), 0,
                   "strncasecmp: full string equal");
    TEST_ASSERT_EQ(strncasecmp("HelloWorld", "helloworld", 5), 0,
                   "strncasecmp: first n equal");

    // Different within n
    TEST_ASSERT_TRUE(strncasecmp("abc", "abd", 3) < 0,
                   "strncasecmp: differ at n");

    // n = 0 (always equal)
    TEST_ASSERT_EQ(strncasecmp("ABC", "def", 0), 0,
                   "strncasecmp: n=0 always equal");

    // Case with early termination
    TEST_ASSERT_EQ(strncasecmp("abc", "ABC", 2), 0,
                   "strncasecmp: equal within limit");
    TEST_ASSERT_TRUE(strncasecmp("ABx", "aby", 3) < 0,
                   "strncasecmp: x < y at position 2");

    TEST_PASS("strncasecmp: all tests passed");
}

// ============================================================================
// tolower Tests
// ============================================================================

static void test_tolower(void) {
    TEST_INFO("Testing tolower...");

    // Uppercase to lowercase
    TEST_ASSERT_EQ(tolower('A'), 'a', "tolower: A -> a");
    TEST_ASSERT_EQ(tolower('Z'), 'z', "tolower: Z -> z");

    // Already lowercase
    TEST_ASSERT_EQ(tolower('a'), 'a', "tolower: a stays a");
    TEST_ASSERT_EQ(tolower('z'), 'z', "tolower: z stays z");

    // Non-alphabetic characters unchanged
    TEST_ASSERT_EQ(tolower('0'), '0', "tolower: digit unchanged");
    TEST_ASSERT_EQ(tolower(' '), ' ', "tolower: space unchanged");
    TEST_ASSERT_EQ(tolower('!'), '!', "tolower: special char unchanged");

    // All uppercase letters - just test a few
    char result = tolower('B');
    TEST_ASSERT_EQ(result, 'b', "tolower: B -> b");

    TEST_PASS("tolower: all tests passed");
}

// ============================================================================
// strchr Tests
// ============================================================================

static void test_strchr(void) {
    TEST_INFO("Testing strchr...");

    const char* str = "hello world";

    // Find existing characters
    char* result = strchr(str, 'o');
    TEST_ASSERT_NOT_NULL(result, "strchr: find 'o'");
    TEST_ASSERT_EQ(result - str, 4, "strchr: 'o' at correct position");

    result = strchr(str, 'h');
    TEST_ASSERT_NOT_NULL(result, "strchr: find 'h'");
    TEST_ASSERT_EQ(result - str, 0, "strchr: 'h' at position 0");

    result = strchr(str, 'd');
    TEST_ASSERT_NOT_NULL(result, "strchr: find 'd'");
    TEST_ASSERT_EQ(result - str, 10, "strchr: 'd' at position 10");

    // Find null terminator
    result = strchr(str, '\0');
    TEST_ASSERT_NOT_NULL(result, "strchr: find null terminator");
    TEST_ASSERT_EQ(*result, '\0', "strchr: null terminator value");

    // Not found
    result = strchr(str, 'x');
    TEST_ASSERT_NULL(result, "strchr: 'x' not found");

    // Empty string
    result = strchr("", '\0');
    TEST_ASSERT_NOT_NULL(result, "strchr: empty string null terminator");

    result = strchr("", 'a');
    TEST_ASSERT_NULL(result, "strchr: empty string no char");

    // First occurrence (not last)
    str = "hello";
    result = strchr(str, 'l');
    TEST_ASSERT_NOT_NULL(result, "strchr: find first 'l'");
    TEST_ASSERT_EQ(result - str, 2, "strchr: first 'l' at position 2");

    TEST_PASS("strchr: all tests passed");
}

// ============================================================================
// strrchr Tests
// ============================================================================

static void test_strrchr(void) {
    TEST_INFO("Testing strrchr...");

    const char* str = "hello world";

    // Find existing characters (last occurrence)
    char* result = strrchr(str, 'o');
    TEST_ASSERT_NOT_NULL(result, "strrchr: find last 'o'");
    TEST_ASSERT_EQ(result - str, 7, "strrchr: last 'o' at position 7");

    result = strrchr(str, 'l');
    TEST_ASSERT_NOT_NULL(result, "strrchr: find last 'l'");
    TEST_ASSERT_EQ(result - str, 9, "strrchr: last 'l' at position 9");

    // Single occurrence
    result = strrchr(str, 'h');
    TEST_ASSERT_NOT_NULL(result, "strrchr: find 'h'");
    TEST_ASSERT_EQ(result - str, 0, "strrchr: 'h' at position 0");

    // Find null terminator
    result = strrchr(str, '\0');
    TEST_ASSERT_NOT_NULL(result, "strrchr: find null terminator");
    TEST_ASSERT_EQ(*result, '\0', "strrchr: null terminator value");

    // Not found
    result = strrchr(str, 'x');
    TEST_ASSERT_NULL(result, "strrchr: 'x' not found");

    // Empty string
    result = strrchr("", '\0');
    TEST_ASSERT_NOT_NULL(result, "strrchr: empty string null terminator");

    result = strrchr("", 'a');
    TEST_ASSERT_NULL(result, "strrchr: empty string no char");

    TEST_PASS("strrchr: all tests passed");
}

// ============================================================================
// strstr Tests
// ============================================================================

static void test_strstr(void) {
    TEST_INFO("Testing strstr...");

    const char* haystack = "hello world";

    // Find existing substring
    char* result = strstr(haystack, "world");
    TEST_ASSERT_NOT_NULL(result, "strstr: find 'world'");
    TEST_ASSERT_EQ(result - haystack, 6, "strstr: 'world' at position 6");

    result = strstr(haystack, "hello");
    TEST_ASSERT_NOT_NULL(result, "strstr: find 'hello'");
    TEST_ASSERT_EQ(result - haystack, 0, "strstr: 'hello' at position 0");

    result = strstr(haystack, "lo wo");
    TEST_ASSERT_NOT_NULL(result, "strstr: find 'lo wo'");
    TEST_ASSERT_EQ(result - haystack, 3, "strstr: 'lo wo' at position 3");

    // Empty needle (returns haystack)
    result = strstr(haystack, "");
    TEST_ASSERT_NOT_NULL(result, "strstr: empty needle");
    TEST_ASSERT_EQ(result, haystack, "strstr: empty needle returns haystack");

    // Not found
    result = strstr(haystack, "xyz");
    TEST_ASSERT_NULL(result, "strstr: 'xyz' not found");

    result = strstr(haystack, "hello world!");
    TEST_ASSERT_NULL(result, "strstr: longer needle not found");

    // Single char
    result = strstr(haystack, "w");
    TEST_ASSERT_NOT_NULL(result, "strstr: single char 'w'");
    TEST_ASSERT_EQ(result - haystack, 6, "strstr: 'w' at position 6");

    // Same string
    result = strstr(haystack, haystack);
    TEST_ASSERT_NOT_NULL(result, "strstr: same string");
    TEST_ASSERT_EQ(result, haystack, "strstr: same string returns start");

    TEST_PASS("strstr: all tests passed");
}

// ============================================================================
// strpbrk Tests
// ============================================================================

static void test_strpbrk(void) {
    TEST_INFO("Testing strpbrk...");

    const char* str = "hello world";

    // Find matching characters
    char* result = strpbrk(str, "xyz");
    TEST_ASSERT_NULL(result, "strpbrk: no match");

    result = strpbrk(str, "ow");
    TEST_ASSERT_NOT_NULL(result, "strpbrk: find 'o' or 'w'");
    TEST_ASSERT_EQ(result - str, 4, "strpbrk: first match at position 4");

    result = strpbrk(str, "de");
    TEST_ASSERT_NOT_NULL(result, "strpbrk: find 'd' or 'e'");
    TEST_ASSERT_EQ(result - str, 1, "strpbrk: first match at position 1");

    // First character match
    result = strpbrk(str, "ha");
    TEST_ASSERT_NOT_NULL(result, "strpbrk: find 'h' or 'a'");
    TEST_ASSERT_EQ(result - str, 0, "strpbrk: first char match");

    // Last character match
    result = strpbrk(str, "d");
    TEST_ASSERT_NOT_NULL(result, "strpbrk: find 'd'");
    TEST_ASSERT_EQ(result - str, 10, "strpbrk: last char match");

    // Multiple in accept set
    result = strpbrk("abc", "cba");
    TEST_ASSERT_NOT_NULL(result, "strpbrk: reverse accept set");
    TEST_ASSERT_EQ(*result, 'a', "strpbrk: first in string matched");

    // Empty accept
    result = strpbrk(str, "");
    TEST_ASSERT_NULL(result, "strpbrk: empty accept set");

    TEST_PASS("strpbrk: all tests passed");
}

// ============================================================================
// strspn Tests
// ============================================================================

static void test_strspn(void) {
    TEST_INFO("Testing strspn...");

    // All characters match
    TEST_ASSERT_EQ(strspn("hello", "helo"), (size_t)5, "strspn: all match");
    TEST_ASSERT_EQ(strspn("aaaaa", "a"), (size_t)5, "strspn: all same char match");

    // Some characters match
    TEST_ASSERT_EQ(strspn("hello world", "helo"), (size_t)5, "strspn: prefix match");
    TEST_ASSERT_EQ(strspn("abc123def", "abc"), (size_t)3, "strspn: 3 chars match");

    // First character doesn't match
    TEST_ASSERT_EQ(strspn("xyz", "abc"), (size_t)0, "strspn: no match at start");

    // Empty string
    TEST_ASSERT_EQ(strspn("", "abc"), (size_t)0, "strspn: empty string");

    // Empty accept
    TEST_ASSERT_EQ(strspn("hello", ""), (size_t)0, "strspn: empty accept");

    // Single char match
    TEST_ASSERT_EQ(strspn("a test", "a"), (size_t)1, "strspn: single char then space");

    // All match including duplicates in accept
    TEST_ASSERT_EQ(strspn("abc", "aaabbbccc"), (size_t)3, "strspn: duplicates in accept");

    TEST_PASS("strspn: all tests passed");
}

// ============================================================================
// strcspn Tests
// ============================================================================

static void test_strcspn(void) {
    TEST_INFO("Testing strcspn...");

    // No reject chars match
    TEST_ASSERT_EQ(strcspn("hello", "xyz"), (size_t)5, "strcspn: no reject match");
    TEST_ASSERT_EQ(strcspn("abc", "def"), (size_t)3, "strcspn: all pass through");

    // Reject at first position
    TEST_ASSERT_EQ(strcspn("hello", "h"), (size_t)0, "strcspn: reject at start");

    // Reject in middle
    TEST_ASSERT_EQ(strcspn("hello world", " "), (size_t)5, "strcspn: space reject");
    TEST_ASSERT_EQ(strcspn("abcde", "d"), (size_t)3, "strcspn: reject at position 3");

    // Reject at end
    TEST_ASSERT_EQ(strcspn("abcd", "d"), (size_t)3, "strcspn: reject at end (before null)");

    // Empty string
    TEST_ASSERT_EQ(strcspn("", "abc"), (size_t)0, "strcspn: empty string");

    // Empty reject
    TEST_ASSERT_EQ(strcspn("hello", ""), (size_t)5, "strcspn: empty reject set");

    // Multiple reject chars
    TEST_ASSERT_EQ(strcspn("abcdef", "bd"), (size_t)1, "strcspn: first reject at 1");

    TEST_PASS("strcspn: all tests passed");
}

// ============================================================================
// strtok_r Tests
// ============================================================================

static void test_strtok_r(void) {
    TEST_INFO("Testing strtok_r...");

    char str[] = "hello,world,tokens";
    char* saveptr = NULL;
    char* token;

    // First token
    token = strtok_r(str, ",", &saveptr);
    TEST_ASSERT_NOT_NULL(token, "strtok_r: first token");
    TEST_ASSERT_STR_EQ(token, "hello", "strtok_r: first token value");

    // Second token
    token = strtok_r(NULL, ",", &saveptr);
    TEST_ASSERT_NOT_NULL(token, "strtok_r: second token");
    TEST_ASSERT_STR_EQ(token, "world", "strtok_r: second token value");

    // Third token
    token = strtok_r(NULL, ",", &saveptr);
    TEST_ASSERT_NOT_NULL(token, "strtok_r: third token");
    TEST_ASSERT_STR_EQ(token, "tokens", "strtok_r: third token value");

    // No more tokens
    token = strtok_r(NULL, ",", &saveptr);
    TEST_ASSERT_NULL(token, "strtok_r: no more tokens");

    // Multiple delimiters
    char str2[] = "hello,,,world";
    saveptr = NULL;
    token = strtok_r(str2, ",", &saveptr);
    TEST_ASSERT_STR_EQ(token, "hello", "strtok_r: multiple delims first");

    token = strtok_r(NULL, ",", &saveptr);
    TEST_ASSERT_STR_EQ(token, "world", "strtok_r: multiple delims after skip");

    // Different delimiters
    char str3[] = "hello world,tokens;test";
    saveptr = NULL;
    token = strtok_r(str3, " ,;", &saveptr);
    TEST_ASSERT_STR_EQ(token, "hello", "strtok_r: multi-char delims first");

    token = strtok_r(NULL, " ,;", &saveptr);
    TEST_ASSERT_STR_EQ(token, "world", "strtok_r: multi-char delims second");

    // Leading delimiters
    char str4[] = ",,hello";
    saveptr = NULL;
    token = strtok_r(str4, ",", &saveptr);
    TEST_ASSERT_STR_EQ(token, "hello", "strtok_r: leading delims skipped");

    // Trailing delimiters
    char str5[] = "hello,,";
    saveptr = NULL;
    token = strtok_r(str5, ",", &saveptr);
    TEST_ASSERT_STR_EQ(token, "hello", "strtok_r: trailing delims");
    token = strtok_r(NULL, ",", &saveptr);
    TEST_ASSERT_NULL(token, "strtok_r: no token after trailing delims");

    // Empty string
    char str6[] = "";
    saveptr = NULL;
    token = strtok_r(str6, ",", &saveptr);
    TEST_ASSERT_NULL(token, "strtok_r: empty string");

    TEST_PASS("strtok_r: all tests passed");
}

// ============================================================================
// strtok Tests
// ============================================================================

static void test_strtok(void) {
    TEST_INFO("Testing strtok...");

    char str[] = "one-two-three";
    char* token;

    // First token
    token = strtok(str, "-");
    TEST_ASSERT_NOT_NULL(token, "strtok: first token");
    TEST_ASSERT_STR_EQ(token, "one", "strtok: first token value");

    // Second token
    token = strtok(NULL, "-");
    TEST_ASSERT_NOT_NULL(token, "strtok: second token");
    TEST_ASSERT_STR_EQ(token, "two", "strtok: second token value");

    // Third token
    token = strtok(NULL, "-");
    TEST_ASSERT_NOT_NULL(token, "strtok: third token");
    TEST_ASSERT_STR_EQ(token, "three", "strtok: third token value");

    // No more tokens
    token = strtok(NULL, "-");
    TEST_ASSERT_NULL(token, "strtok: no more tokens");

    TEST_PASS("strtok: all tests passed");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
    TEST_RUNNER_BEGIN("String Utility Functions");

    // Basic operations
    test_strlen();
    test_strnlen();
    test_strcpy();
    test_strncpy();

    // Comparison
    test_strcmp();
    test_strncmp();
    test_strcasecmp();
    test_strncasecmp();
    test_tolower();

    // Search
    test_strchr();
    test_strrchr();
    test_strstr();
    test_strpbrk();
    test_strspn();
    test_strcspn();

    // Tokenization
    test_strtok_r();
    test_strtok();

    TEST_RUNNER_END();
}
