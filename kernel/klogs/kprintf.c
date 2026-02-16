/**
 * @file kprintf.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Kernel printf implementation with backend support
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "kprintf.h"
#include "backends/serial_backends.h"
#include "base/strhelpers.h"
#include "kprintf_config.h"

// Buffer for formatted output
static char buffer[KPRINTF_BUFFER_SIZE];

// Current log level filter
static klog_level_t g_log_level = KPRINTF_DEFAULT_FILTERED_LOGLEVEL;

// Format buffer for number conversion
static char g_format_buffer[32];

/**
 * @brief Minimal vsnprintf implementation
 */
static int format_string(char* buffer, size_t size, const char* format, va_list args) {
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

        // Handle format specifiers
        switch (*p) {
            case '%':
                buffer[pos++] = '%';
                break;
            case 'c': {
                char c = (char)va_arg(args, int);
                buffer[pos++] = c;
                break;
            }
            case 's': {
                const char* s = va_arg(args, const char*);
                if (s == NULL) {
                    s = "(null)";
                }
                while (*s != '\0' && pos < size - 1) {
                    buffer[pos++] = *s++;
                }
                break;
            }
            case 'd':
            case 'i': {
                int val = va_arg(args, int);
                itoa(val, g_format_buffer, 10);
                char* s = g_format_buffer;
                while (*s != '\0' && pos < size - 1) {
                    buffer[pos++] = *s++;
                }
                break;
            }
            case 'u': {
                unsigned int val = va_arg(args, unsigned int);
                uitoa(val, g_format_buffer, 10);
                char* s = g_format_buffer;
                while (*s != '\0' && pos < size - 1) {
                    buffer[pos++] = *s++;
                }
                break;
            }
            case 'x': {
                unsigned int val = va_arg(args, unsigned int);
                uitoa(val, g_format_buffer, 16);
                char* s = g_format_buffer;
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
                uitoa((unsigned int)(uintptr_t)ptr, g_format_buffer, 16);
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

bool klog_init(klog_backend_t backend) {
    // Initialize serial backend if requested
    if (backend == KLOG_BACKEND_SERIAL) {
        if (!klog_serial_backend_init()) {
            return false;
        }
        klog_register_backend(KLOG_BACKEND_SERIAL, klog_serial_backend_get_ops());
    }

    klog_set_default_backend(backend);

    klog_trace("Klog Finished, attempt to send followings...\n");
    klog_trace("========================================================");
    klog_trace("\tCurrent Filtered Level: %s\n", klog_level_name(g_log_level));
    klog_trace("\tCached printf size: %d", KPRINTF_BUFFER_SIZE);
    klog_trace("========================================================");

    return true;
}

void kvprintf(klog_backend_t backend, const char* format, va_list args) {
    const KLogBackendOps* ops = klog_get_backend_ops(backend);
    if (ops == NULL || !ops->is_ready()) {
        return;
    }

    static char buffer[KPRINTF_BUFFER_SIZE];
    format_string(buffer, KPRINTF_BUFFER_SIZE, format, args);
    // Use level -1 for kprintf (no specific level, default color)
    ops->process(buffer, -1);
}

void kprintf(klog_backend_t backend, const char* format, ...) {
    va_list args;
    va_start(args, format);
    kvprintf(backend, format, args);
    va_end(args);
}

void klog_set_level(klog_level_t level) {
    g_log_level = level;
}

klog_level_t klog_get_level(void) {
    return g_log_level;
}

const char* klog_level_name(klog_level_t level) {
    switch (level) {
        case KLOG_LEVEL_TRACE:
            return "TRACE";
        case KLOG_LEVEL_DEBUG:
            return "DEBUG";
        case KLOG_LEVEL_INFO:
            return "INFO ";
        case KLOG_LEVEL_WARN:
            return "WARN ";
        case KLOG_LEVEL_ERROR:
            return "ERROR";
        default:
            return "?????";
    }
}

void klog_log(klog_level_t level, const char* format, ...) {
    // Check log level
    if (level < g_log_level) {
        return;
    }

    klog_backend_t backend = klog_get_default_backend();
    const KLogBackendOps* ops = klog_get_backend_ops(backend);
    if (ops == NULL || !ops->is_ready()) {
        return;
    }

    // Format: "[LEVEL] message\n"
    int pos = 0;
    buffer[pos++] = '[';

    const char* level_str = klog_level_name(level);
    for (int i = 0; i < 5 && level_str[i] != '\0'; i++) {
        buffer[pos++] = level_str[i];
    }

    buffer[pos++] = ']';
    buffer[pos++] = ' ';

    // Format the user message
    va_list args;
    va_start(args, format);
    int len = format_string(buffer + pos, KPRINTF_BUFFER_SIZE - pos - 2, format, args);
    va_end(args);

    pos += len;

    // Add newline
    buffer[pos++] = '\n';
    buffer[pos] = '\0';

    ops->process(buffer, level);
}
