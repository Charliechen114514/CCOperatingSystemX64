/**
 * @file string.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Basic C-style string utility function declarations.
 *
 * This header provides common string manipulation, comparison,
 * search, and tokenization function prototypes for a custom libc
 * or kernel environment.
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include "defines/types.h"

/* ================= Basic string operations ================= */

/**
 * @brief Compute the length of a null-terminated string.
 *
 * Counts the number of characters in the string before the first
 * null terminator.
 *
 * @param s Pointer to the input string.
 * @return Length of the string (excluding the null terminator).
 */
size_t strlen(const char* s);

/**
 * @brief Compute the length of a string with a maximum bound.
 *
 * Counts at most @p maxlen characters and stops early if a null
 * terminator is encountered.
 *
 * @param s Pointer to the input string.
 * @param maxlen Maximum number of characters to examine.
 * @return Length of the string up to @p maxlen.
 */
size_t strnlen(const char* s, size_t maxlen);

/**
 * @brief Copy a null-terminated string.
 *
 * Copies the string pointed to by @p src (including the null
 * terminator) into @p dest.
 *
 * @param dest Destination buffer.
 * @param src Source string.
 * @return Pointer to @p dest.
 */
char* strcpy(char* dest, const char* src);

/**
 * @brief Copy a string with length limit.
 *
 * Copies at most @p n characters from @p src into @p dest.
 * If @p src is shorter than @p n, the remainder of @p dest
 * is padded with null bytes.
 *
 * @param dest Destination buffer.
 * @param src Source string.
 * @param n Maximum number of characters to copy.
 * @return Pointer to @p dest.
 */
char* strncpy(char* dest, const char* src, size_t n);

/* ================= String comparison ================= */

/**
 * @brief Compare two strings lexicographically.
 *
 * @param s1 First string.
 * @param s2 Second string.
 * @return An integer less than, equal to, or greater than zero
 *         if @p s1 is found, respectively, to be less than,
 *         to match, or be greater than @p s2.
 */
int strcmp(const char* s1, const char* s2);

/**
 * @brief Compare two strings up to a maximum length.
 *
 * @param s1 First string.
 * @param s2 Second string.
 * @param n Maximum number of characters to compare.
 * @return Comparison result as in strcmp.
 */
int strncmp(const char* s1, const char* s2, size_t n);

/**
 * @brief Compare two strings ignoring case.
 *
 * @param s1 First string.
 * @param s2 Second string.
 * @return Comparison result ignoring character case.
 */
int strcasecmp(const char* s1, const char* s2);

/**
 * @brief Compare two strings ignoring case with length limit.
 *
 * @param s1 First string.
 * @param s2 Second string.
 * @param n Maximum number of characters to compare.
 * @return Comparison result ignoring case up to @p n characters.
 */
int strncasecmp(const char* s1, const char* s2, size_t n);

/**
 * @brief ToLowers
 *
 * @param c
 * @return char
 */
char tolower(char c);

/* ================= String search ================= */

/**
 * @brief Find first occurrence of a character in a string.
 *
 * @param s Input string.
 * @param c Character to search for (converted to char).
 * @return Pointer to the first occurrence in @p s,
 *         or NULL if not found.
 */
char* strchr(const char* s, int c);

/**
 * @brief Find last occurrence of a character in a string.
 *
 * @param s Input string.
 * @param c Character to search for.
 * @return Pointer to the last occurrence in @p s,
 *         or NULL if not found.
 */
char* strrchr(const char* s, int c);

/**
 * @brief Find a substring within a string.
 *
 * @param haystack String to search in.
 * @param needle Substring to search for.
 * @return Pointer to the first occurrence of @p needle
 *         in @p haystack, or NULL if not found.
 */
char* strstr(const char* haystack, const char* needle);

/**
 * @brief Search a string for any of a set of characters.
 *
 * @param s Input string.
 * @param accept Set of characters to match.
 * @return Pointer to the first matching character in @p s,
 *         or NULL if none found.
 */
char* strpbrk(const char* s, const char* accept);

/**
 * @brief Compute length of initial segment containing only accepted characters.
 *
 * @param s Input string.
 * @param accept Set of accepted characters.
 * @return Number of characters in the initial segment of @p s
 *         consisting only of characters from @p accept.
 */
size_t strspn(const char* s, const char* accept);

/**
 * @brief Compute length of initial segment containing no rejected characters.
 *
 * @param s Input string.
 * @param reject Set of rejected characters.
 * @return Number of characters in the initial segment of @p s
 *         containing none of the characters in @p reject.
 */
size_t strcspn(const char* s, const char* reject);

/* ================= String tokenization ================= */

/**
 * @brief Split string into tokens using delimiters.
 *
 * Breaks the string into a sequence of tokens separated by any of the
 * characters in @p delim. This function is not thread-safe.
 *
 * @param str Input string (modified in-place). Pass NULL to continue.
 * @param delim Delimiter character set.
 * @return Pointer to next token, or NULL if no more tokens.
 */
char* strtok(char* str, const char* delim);

/**
 * @brief Reentrant string tokenizer.
 *
 * Thread-safe version of strtok using caller-provided context.
 *
 * @param str Input string (modified in-place). Pass NULL to continue.
 * @param delim Delimiter character set.
 * @param saveptr Pointer to tokenizer state.
 * @return Pointer to next token, or NULL if no more tokens.
 */
char* strtok_r(char* str, const char* delim, char** saveptr);
