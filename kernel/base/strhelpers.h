/**
 * @file strhelpers.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once
#include "defines/types.h"

/**
 * @brief is space?
 *
 * @param ch
 * @return true: isspace
 * @return false: not
 */
bool isspace(char ch);

/**
 * @brief is digit?
 *
 * @param ch
 * @return true
 * @return false
 */
bool isdigit(char ch);

/**
 * @brief Reverse the string given with target lens
 *
 * @param str
 * @param len
 */
void reverse_str(char* str, int len);
/**
 * @brief Convert a string to a long integer.
 *
 * Converts the initial part of the string in @p nptr to a long integer value according to the given
 * @p base.
 *
 * @param nptr Pointer to the null-terminated string to convert.
 * @param endptr Pointer to a pointer to character, used to store the address of the first
 * unconverted character. Can be NULL if not needed.
 * @param base Numerical base (radix) for the conversion. Range: 2 to 36, or 0 to auto-detect.
 *             If 0, the base is determined by the string prefix: "0x" for hex, "0" for octal,
 * otherwise decimal.
 * @return The converted long integer. Returns 0 if no valid conversion could be performed.
 */
long strtol(const char* nptr, char** endptr, int base);

/**
 * @brief Convert a string to a long long integer.
 *
 * Similar to strtol but returns a long long integer.
 *
 * @param nptr Pointer to the null-terminated string to convert.
 * @param endptr Pointer to a pointer to character, used to store the address of the first
 * unconverted character. Can be NULL if not needed.
 * @param base Numerical base (radix) for the conversion. Range: 2 to 36, or 0 to auto-detect.
 * @return The converted long long integer. Returns 0 if no valid conversion could be performed.
 */
long long strtoll(const char* nptr, char** endptr, int base);

/**
 * @brief Convert a string to an unsigned long integer.
 *
 * Converts the initial part of the string in @p nptr to an unsigned long integer according to the
 * given @p base.
 *
 * @param nptr Pointer to the null-terminated string to convert.
 * @param endptr Pointer to a pointer to character, used to store the address of the first
 * unconverted character. Can be NULL if not needed.
 * @param base Numerical base (radix) for the conversion. Range: 2 to 36, or 0 to auto-detect.
 * @return The converted unsigned long integer. Returns 0 if no valid conversion could be performed.
 */
unsigned long strtoul(const char* nptr, char** endptr, int base);

/**
 * @brief Convert a string to an int.
 *
 * Simplified version of strtol. Assumes base 10 and does not provide error checking.
 *
 * @param nptr Pointer to the null-terminated string to convert.
 * @return The converted integer value. Returns 0 if the string does not represent a valid number.
 */
int atoi(const char* nptr);

/**
 * @brief Convert an integer to a string.
 *
 * Converts the given integer value to a null-terminated string using the specified @p base.
 *
 * @param value The integer value to convert.
 * @param str Pointer to a buffer to store the resulting string. Must be large enough to hold the
 * result.
 * @param base Numerical base (radix) for conversion. Range: 2 to 36.
 * @return Pointer to the resulting string (same as @p str).
 */
char* itoa(int value, char* str, int base);

/**
 * @brief Convert an unsigned integer to a string.
 *
 * Converts the given unsigned integer value to a null-terminated string using the specified @p
 * base.
 *
 * @param value The unsigned integer value to convert.
 * @param str Pointer to a buffer to store the resulting string. Must be large enough to hold the
 * result.
 * @param base Numerical base (radix) for conversion. Range: 2 to 36.
 * @return Pointer to the resulting string (same as @p str).
 */
char* uitoa(unsigned int value, char* str, int base);

/**
 * @brief Convert a signed 64-bit integer to a string.
 *
 * Converts the given signed 64-bit integer value to a null-terminated string using the
 * specified @p base.
 *
 * @param value The 64-bit integer value to convert.
 * @param str Pointer to a buffer to store the resulting string. Must be large enough to hold the
 * result (at least 65 bytes for base 2).
 * @param base Numerical base (radix) for conversion. Range: 2 to 36.
 * @return Pointer to the resulting string (same as @p str).
 */
char* itoa_signed(int64_t value, char* str, int base);

/**
 * @brief Convert an unsigned 64-bit integer to a string.
 *
 * Converts the given unsigned 64-bit integer value to a null-terminated string using the
 * specified @p base.
 *
 * @param value The unsigned 64-bit integer value to convert.
 * @param str Pointer to a buffer to store the resulting string. Must be large enough to hold the
 * result (at least 65 bytes for base 2).
 * @param base Numerical base (radix) for conversion. Range: 2 to 36.
 * @return Pointer to the resulting string (same as @p str).
 */
char* uitoa64(uint64_t value, char* str, int base);
