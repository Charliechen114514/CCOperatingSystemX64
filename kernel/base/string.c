#include "string.h"
#include "assert/assert.h"

size_t strlen(const char* s) {
    // s should never be NULL
    CCOS_DEBUG_ASSERT(s);

    const char* p = s;
    while (*p) {
        p++;
    }
    return (size_t)(p - s);
}

size_t strnlen(const char* s, size_t maxlen) {
    // s should never be NULL
    CCOS_DEBUG_ASSERT(s);

    const char* index_p = s;
    while (*index_p && maxlen > 0) {
        index_p++;
        maxlen--;
    }
    return (size_t)(index_p - s);
}

char* strcpy(char* dest, const char* src) {
    // s should never be NULL
    CCOS_DEBUG_ASSERT(dest && src);
    char* d = dest;
    while ((*d++ = *src++) != '\0') {
        /* Copy until null terminator */
    }
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    // s should never be NULL
    CCOS_DEBUG_ASSERT(dest && src);

    char* d = dest;
    while ((*d++ = *src++) != '\0' && n > 0) {
        /* Copy until null terminator or n > 0 */
        n--;
    }
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    CCOS_DEBUG_ASSERT(s1 && s2);

    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    CCOS_DEBUG_ASSERT(s1 && s2);

    for (; n && *s1 && (*s1 == *s2); --n, ++s1, ++s2)
        ;

    if (n == 0)
        return 0;

    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strcasecmp(const char* s1, const char* s2) {
    CCOS_DEBUG_ASSERT(s1 && s2);

    while (*s1 && *s2) {
        char c1 = tolower(*s1);
        char c2 = tolower(*s2);

        if (c1 != c2)
            return (unsigned char)c1 - (unsigned char)c2;

        ++s1;
        ++s2;
    }

    return (unsigned char)tolower(*s1) - (unsigned char)tolower(*s2);
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
    CCOS_DEBUG_ASSERT(s1 && s2);

    for (; n; --n, ++s1, ++s2) {
        char c1 = tolower(*s1);
        char c2 = tolower(*s2);

        if (c1 != c2)
            return (unsigned char)c1 - (unsigned char)c2;

        if (c1 == '\0')
            return 0;
    }

    return 0;
}

char* strchr(const char* s, int c) {
    CCOS_DEBUG_ASSERT(s);

    unsigned char ch = (unsigned char)c;

    for (; *s; ++s) {
        if ((unsigned char)*s == ch)
            return (char*)s;
    }

    if (ch == '\0')
        return (char*)s;

    return NULL;
}

char* strrchr(const char* s, int c) {
    CCOS_DEBUG_ASSERT(s);

    unsigned char ch = (unsigned char)c;
    const char* last = NULL;

    do {
        if ((unsigned char)*s == ch)
            last = s;
    } while (*s++);

    return (char*)last;
}

char* strstr(const char* haystack, const char* needle) {
    CCOS_DEBUG_ASSERT(haystack && needle);

    if (*needle == '\0')
        return (char*)haystack;

    for (; *haystack; ++haystack) {
        const char* h = haystack;
        const char* n = needle;

        while (*h && *n && (*h == *n)) {
            ++h;
            ++n;
        }

        if (*n == '\0')
            return (char*)haystack;
    }

    return NULL;
}

char* strpbrk(const char* s, const char* accept) {
    CCOS_DEBUG_ASSERT(s && accept);

    for (; *s; ++s) {
        for (const char* a = accept; *a; ++a) {
            if (*s == *a)
                return (char*)s;
        }
    }

    return NULL;
}

size_t strspn(const char* s, const char* accept) {
    CCOS_DEBUG_ASSERT(s && accept);

    size_t count = 0;

    for (; *s; ++s) {
        int ok = 0;
        for (const char* a = accept; *a; ++a) {
            if (*s == *a) {
                ok = 1;
                break;
            }
        }
        if (!ok)
            break;
        ++count;
    }

    return count;
}

size_t strcspn(const char* s, const char* reject) {
    CCOS_DEBUG_ASSERT(s && reject);

    size_t count = 0;

    for (; *s; ++s) {
        for (const char* r = reject; *r; ++r) {
            if (*s == *r)
                return count;
        }
        ++count;
    }

    return count;
}

char* strtok_r(char* str, const char* delim, char** saveptr) {
    CCOS_DEBUG_ASSERT(delim && saveptr);

    char* s = str ? str : *saveptr;
    if (!s)
        return NULL;

    /* skip leading delimiters */
    s += strspn(s, delim);
    if (*s == '\0') {
        *saveptr = NULL;
        return NULL;
    }

    char* token = s;

    /* find end */
    s += strcspn(s, delim);
    if (*s) {
        *s = '\0';
        *saveptr = s + 1;
    } else {
        *saveptr = NULL;
    }

    return token;
}

char* strtok(char* str, const char* delim) {
    static char* save;
    return strtok_r(str, delim, &save);
}

char tolower(char c) {
    if ((unsigned)(c - 'A') <= ('Z' - 'A'))
        c |= 0x20;
    return c;
}