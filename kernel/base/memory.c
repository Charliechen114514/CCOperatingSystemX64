#include "memory.h"
#include "assert/assert.h"

void* memset(void* s, int c, size_t n) {
    CCOS_DEBUG_ASSERT(s);
    char* asByteWrite = (char*)s;
    while (n > 0) {
        asByteWrite[n - 1] = (char)c;
        n--;
    }
    return s;
}

void* memcpy(void* dest, const void* src, size_t n) {
    CCOS_DEBUG_ASSERT(dest && src);
    char* asCharDest = (char*)dest;
    char* asCharSrc = (char*)src;

    while (n > 0) {
        *asCharDest = *asCharSrc;
        asCharDest++;
        asCharSrc++;
        n--;
    }

    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;

    if (d == s || n == 0) {
        return dest;
    }

    if (d > s && d < s + n) {
        d += n;
        s += n;
        while (n--) {
            *(--d) = *(--s);
        }
    } else {
        while (n--) {
            *d++ = *s++;
        }
    }

    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    CCOS_DEBUG_ASSERT(s1 && s2);
    const unsigned char* _s1 = s1;
    const unsigned char* _s2 = s2;

    for (; n && *_s1 == *_s2; --n, ++_s1, ++_s2)
        ;

    if (n == 0)
        return 0;

    return *_s1 - *_s2;
}
