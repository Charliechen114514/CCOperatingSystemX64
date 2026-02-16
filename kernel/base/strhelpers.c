#include "strhelpers.h"
#include "help_macros.h"
#include "string.h"

bool isspace(char ch) {
    if (ch == '\0')
        return false;
    return strchr(" \t\n\v\f\r", ch) != NULL;
}

bool isdigit(char ch) {
    return '0' <= ch && ch <= '9';
}

long strtol(const char* nptr, char** endptr, int base) {
    const char* s = nptr;
    long result = 0;
    bool neg = false;

    // Skip whitespace
    while (isspace((unsigned char)*s))
        s++;
    if (*s == '-') {
        neg = 1; // OK, negatives
        s++;
    } else if (*s == '+') {
        s++;
    }

    if (base == 0) {
        if (*s == '0') { // 16base or 8base
            if (*(s + 1) == 'x' || *(s + 1) == 'X') {
                base = 16;
                s += 2;
            } else {
                base = 8;
                s++;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X'))
            s += 2;
    }

    const char* start = s;
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9')
            digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z')
            digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z')
            digit = *s - 'A' + 10;
        else
            break;

        if (digit >= base)
            break;

        result = result * base + digit;
        s++;
    }

    if (endptr)
        *endptr = (s != start) ? (char*)s : (char*)nptr;

    return neg ? -result : result;
}

long long strtoll(const char* nptr, char** endptr, int base) {
    return (long long)strtol(nptr, endptr, base);
}

unsigned long strtoul(const char* nptr, char** endptr, int base) {
    const char* s = nptr;
    unsigned long result = 0;

    // Skip whitespace
    while (isspace((unsigned char)*s))
        s++;

    // Skip optional '+'
    if (*s == '+')
        s++;
    // optional '-' for unsigned? typically undefined, we just ignore here

    // Detect base if 0
    if (base == 0) {
        if (*s == '0') {
            if (*(s + 1) == 'x' || *(s + 1) == 'X') {
                base = 16;
                s += 2;
            } else {
                base = 8;
                s++;
            }
        } else
            base = 10;
    } else if (base == 16) {
        if (*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X'))
            s += 2;
    }

    const char* start = s;
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9')
            digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z')
            digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z')
            digit = *s - 'A' + 10;
        else
            break;

        if (digit >= base)
            break;

        result = result * base + digit;
        s++;
    }

    if (endptr)
        *endptr = (s != start) ? (char*)s : (char*)nptr;

    return result;
}

/**
 * @brief Simplified string to integer (base 10).
 */
int atoi(const char* nptr) {
    return (int)strtol(nptr, NULL, 10);
}

/**
 * @brief Internal helper: reverse a string in-place.
 */
void reverse_str(char* str, int len) {
    int i = 0, j = len - 1;
    while (i < j) {
        char tmp = str[i];
        str[i] = str[j];
        str[j] = tmp;
        i++;
        j--;
    }
}

/**
 * @brief Convert integer to string in given base.
 */
char* itoa(int value, char* str, int base) {
    if (base < 2 || base > 36) {
        str[0] = '\0';
        return str;
    }

    int i = 0;
    int neg = 0;
    unsigned int uval;

    if (value < 0 && base == 10) {
        neg = 1;
        uval = (unsigned int)(-value);
    } else
        uval = (unsigned int)value;

    do {
        int digit = uval % base;
        str[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        uval /= base;
    } while (uval != 0);

    if (neg)
        str[i++] = '-';

    str[i] = '\0';
    reverse_str(str, i);
    return str;
}

/**
 * @brief Convert unsigned integer to string in given base.
 */
char* uitoa(unsigned int value, char* str, int base) {
    if (base < 2 || base > 36) {
        str[0] = '\0';
        return str;
    }

    int i = 0;
    unsigned int uval = value;

    do {
        int digit = uval % base;
        str[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        uval /= base;
    } while (uval != 0);

    str[i] = '\0';
    reverse_str(str, i);
    return str;
}