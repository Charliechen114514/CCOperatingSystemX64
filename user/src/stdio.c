/* ==============================================================================
 * CCOS User Library - Standard I/O Implementation
 * ==============================================================================
 *
 * Implementation of standard I/O functions.
 *
 * ==============================================================================
 */

#include "stdio.h"
#include "unistd.h"

/* Helper: Calculate string length */
static size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

/* ============================================================================
 * Simple printf implementation
 * ============================================================================ */

static int print_int(char* buf, int value, int base, int is_signed) {
    char tmp[32];
    int i = 0;
    int is_negative = 0;

    if (value == 0) {
        buf[0] = '0';
        return 1;
    }

    if (is_signed && value < 0) {
        is_negative = 1;
        value = -value;
    }

    unsigned int uvalue = (unsigned int)value;

    while (uvalue > 0) {
        int digit = uvalue % base;
        tmp[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        uvalue /= base;
    }

    int len = 0;
    if (is_negative) {
        buf[len++] = '-';
    }

    while (i > 0) {
        buf[len++] = tmp[--i];
    }

    return len;
}

static int print_hex(char* buf, unsigned long long value, int upper) {
    char tmp[32];
    int i = 0;

    if (value == 0) {
        buf[0] = '0';
        return 1;
    }

    while (value > 0) {
        int digit = value & 0xF;
        tmp[i++] = (digit < 10) ? ('0' + digit) : ((upper ? 'A' : 'a') + digit - 10);
        value >>= 4;
    }

    int len = 0;
    while (i > 0) {
        buf[len++] = tmp[--i];
    }

    return len;
}

/* va_list is typically defined by the compiler */
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)

static int vfprintf(int fd, const char* format, va_list args) {
    char buf[512];
    int buf_idx = 0;

    while (*format && buf_idx < (int)sizeof(buf) - 1) {
        if (*format != '%') {
            buf[buf_idx++] = *format++;
            continue;
        }

        format++; /* Skip '%' */

        if (*format == '\0') break;

        /* Handle %% */
        if (*format == '%') {
            buf[buf_idx++] = '%';
            format++;
            continue;
        }

        /* Parse format specifier */
        /* Skip zero padding and width modifiers (not implemented) */
        while (*format == '0' || (*format >= '0' && *format <= '9')) {
            format++;
        }

        /* Check for long modifier (not implemented) */
        if (*format == 'l') {
            format++;
            if (*format == 'l') {
                format++; /* 'll' -> 64-bit */
            }
        }

        char spec = *format++;
        char tmp[64];
        int tmp_len = 0;

        switch (spec) {
            case 'c':
                buf[buf_idx++] = (char)va_arg(args, int);
                break;
            case 's': {
                const char* s = va_arg(args, const char*);
                if (s == NULL) s = "(null)";
                int s_len = strlen(s);
                for (int i = 0; i < s_len && buf_idx < (int)sizeof(buf) - 1; i++) {
                    buf[buf_idx++] = s[i];
                }
                break;
            }
            case 'd':
            case 'i':
                tmp_len = print_int(tmp, va_arg(args, int), 10, 1);
                for (int i = 0; i < tmp_len && buf_idx < (int)sizeof(buf) - 1; i++) {
                    buf[buf_idx++] = tmp[i];
                }
                break;
            case 'u':
                tmp_len = print_int(tmp, va_arg(args, int), 10, 0);
                for (int i = 0; i < tmp_len && buf_idx < (int)sizeof(buf) - 1; i++) {
                    buf[buf_idx++] = tmp[i];
                }
                break;
            case 'x':
                tmp_len = print_hex(tmp, va_arg(args, unsigned int), 0);
                for (int i = 0; i < tmp_len && buf_idx < (int)sizeof(buf) - 1; i++) {
                    buf[buf_idx++] = tmp[i];
                }
                break;
            case 'X':
                tmp_len = print_hex(tmp, va_arg(args, unsigned int), 1);
                for (int i = 0; i < tmp_len && buf_idx < (int)sizeof(buf) - 1; i++) {
                    buf[buf_idx++] = tmp[i];
                }
                break;
            case 'p': {
                void* ptr = va_arg(args, void*);
                buf[buf_idx++] = '0';
                buf[buf_idx++] = 'x';
                tmp_len = print_hex(tmp, (unsigned long long)ptr, 0);
                for (int i = 0; i < tmp_len && buf_idx < (int)sizeof(buf) - 1; i++) {
                    buf[buf_idx++] = tmp[i];
                }
                break;
            }
            default:
                /* Unknown specifier, just output it */
                buf[buf_idx++] = spec;
                break;
        }
    }

    buf[buf_idx] = '\0';
    return write(fd, buf, buf_idx);
}

int fprintf(int fd, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vfprintf(fd, format, args);
    va_end(args);
    return ret;
}

int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vfprintf(STDOUT_FILENO, format, args);
    va_end(args);
    return ret;
}

int fputs(int fd, const char* s) {
    if (s == NULL) return -1;
    int len = strlen(s);
    return write(fd, s, len);
}

int puts(const char* s) {
    int ret = fputs(STDOUT_FILENO, s);
    if (ret >= 0) {
        /* Add newline */
        write(STDOUT_FILENO, "\n", 1);
        ret++;
    }
    return ret;
}

int fputc(int fd, int c) {
    char ch = (char)c;
    return write(fd, &ch, 1);
}

int putchar(int c) {
    return fputc(STDOUT_FILENO, c);
}
