/**
 * @file format.c
 * @brief Internal formatting utilities for klogs subsystem
 */

#include "format.h"
#include "base/strhelpers.h"
#include "kprintf_config.h"

// Format buffer for number conversion (shared across the module)
// Size is configured via KPRINTF_FORMAT_BUFFER_SIZE
static char g_format_buffer[KPRINTF_FORMAT_BUFFER_SIZE];

/**
 * @brief Format string to buffer (internal vsnprintf implementation)
 *
 * Supports: %c, %s, %d/%i, %u, %x, %lx, %p
 * Width/flags: %Nx, %0Nx, %-Ns (where N is width, - means left align)
 */
int klog_format_string(char* buffer, size_t size, const char* format, va_list args) {
    if (buffer == NULL || size == 0) {
        return -1;
    }

    size_t pos = 0;
    const char* p = format;

    while (*p != '\0' && pos < size - 1) {
        if (*p != '%') {
            buffer[pos++] = *p++;
            continue;
        }

        p++; // Skip '%'

        if (*p == '\0') {
            buffer[pos++] = '%';
            break;
        }

        // Parse width and flags: %0Nx or %Nx or %-Ns
        int width = 0;
        int pad_with_zero = 0;
        int left_align = 0;

        // Check for left-align flag
        if (*p == '-') {
            left_align = 1;
            p++;
        }

        // Check for zero-padding flag (only for right-align)
        if (*p == '0' && !left_align) {
            pad_with_zero = 1;
            p++;
        }

        // Parse width digits
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        // Length modifier: 'l' for long (64-bit on x86_64)
        int is_long = 0;
        if (*p == 'l') {
            is_long = 1;
            p++;
        }

        // Handle format specifiers
        switch (*p) {
            case '%':
                buffer[pos++] = '%';
                break;
            case 'c': {
                char c = (char)va_arg(args, int);
                if (width > 1) {
                    if (left_align) {
                        buffer[pos++] = c;
                        for (int i = 1; i < width && pos < size - 1; i++) {
                            buffer[pos++] = ' ';
                        }
                    } else {
                        for (int i = 1; i < width && pos < size - 1; i++) {
                            buffer[pos++] = ' ';
                        }
                        if (pos < size - 1)
                            buffer[pos++] = c;
                    }
                } else {
                    buffer[pos++] = c;
                }
                break;
            }
            case 's': {
                const char* s = va_arg(args, const char*);
                if (s == NULL) {
                    s = "(null)";
                }
                int len = 0;
                const char* tmp = s;
                while (*tmp != '\0') {
                    len++;
                    tmp++;
                }

                if (width > len) {
                    int pad = width - len;
                    if (left_align) {
                        while (*s != '\0' && pos < size - 1) {
                            buffer[pos++] = *s++;
                        }
                        for (int i = 0; i < pad && pos < size - 1; i++) {
                            buffer[pos++] = ' ';
                        }
                    } else {
                        for (int i = 0; i < pad && pos < size - 1; i++) {
                            buffer[pos++] = ' ';
                        }
                        while (*s != '\0' && pos < size - 1) {
                            buffer[pos++] = *s++;
                        }
                    }
                } else {
                    while (*s != '\0' && pos < size - 1) {
                        buffer[pos++] = *s++;
                    }
                }
                break;
            }
            case 'd':
            case 'i': {
                int64_t val = is_long ? va_arg(args, int64_t) : va_arg(args, int);
                itoa_signed(val, g_format_buffer, 10);

                int len = 0;
                char* s = g_format_buffer;
                while (*s != '\0') {
                    len++;
                    s++;
                }

                if (width > len && !left_align) {
                    char pad_char = pad_with_zero ? '0' : ' ';
                    int pad_count = width - len;
                    for (int i = 0; i < pad_count && pos < size - 1; i++) {
                        buffer[pos++] = pad_char;
                    }
                }

                s = g_format_buffer;
                while (*s != '\0' && pos < size - 1) {
                    buffer[pos++] = *s++;
                }

                if (width > len && left_align) {
                    int pad_count = width - len;
                    for (int i = 0; i < pad_count && pos < size - 1; i++) {
                        buffer[pos++] = ' ';
                    }
                }
                break;
            }
            case 'u': {
                uint64_t val = is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                uitoa64(val, g_format_buffer, 10);

                int len = 0;
                char* s = g_format_buffer;
                while (*s != '\0') {
                    len++;
                    s++;
                }

                if (width > len && !left_align) {
                    char pad_char = pad_with_zero ? '0' : ' ';
                    int pad_count = width - len;
                    for (int i = 0; i < pad_count && pos < size - 1; i++) {
                        buffer[pos++] = pad_char;
                    }
                }

                s = g_format_buffer;
                while (*s != '\0' && pos < size - 1) {
                    buffer[pos++] = *s++;
                }

                if (width > len && left_align) {
                    int pad_count = width - len;
                    for (int i = 0; i < pad_count && pos < size - 1; i++) {
                        buffer[pos++] = ' ';
                    }
                }
                break;
            }
            case 'x': {
                uint64_t val = is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                uitoa64(val, g_format_buffer, 16);

                // Handle width padding
                int len = 0;
                char* s = g_format_buffer;
                while (*s != '\0') {
                    len++;
                    s++;
                }

                // Pad with spaces or zeros if needed
                if (width > len) {
                    char pad_char = pad_with_zero ? '0' : ' ';
                    int pad_count = width - len;
                    for (int i = 0; i < pad_count && pos < size - 1; i++) {
                        buffer[pos++] = pad_char;
                    }
                }

                // Output the number
                s = g_format_buffer;
                while (*s != '\0' && pos < size - 1) {
                    buffer[pos++] = *s++;
                }
                break;
            }
            case 'p': {
                void* ptr = va_arg(args, void*);
                buffer[pos++] = '0';
                if (pos < size - 1)
                    buffer[pos++] = 'x';
                uitoa64((uint64_t)(uintptr_t)ptr, g_format_buffer, 16);
                char* s = g_format_buffer;
                while (*s != '\0' && pos < size - 1) {
                    buffer[pos++] = *s++;
                }
                break;
            }
            default:
                // Unknown format, just output the char
                buffer[pos++] = *p;
                break;
        }
        p++;
    }

    buffer[pos] = '\0';
    return (int)pos;
}
