/**
 * @file ksnprintf.c
 * @brief Kernel snprintf implementation for string formatting
 */

#include "ksnprintf.h"
#include "private/format.h"

int kvsnprintf(char* buffer, size_t size, const char* format, va_list args) {
    return klog_format_string(buffer, size, format, args);
}

int ksnprintf(char* buffer, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = kvsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}
