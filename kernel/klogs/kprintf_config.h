/**
 * @file kprintf_config.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Configs for the Default kprintf backends
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */

/**
 * @brief How many backends shell we supports
 *
 */
#define KPRINTF_MAX_BACKEND_N (4)
#if KPRINTF_MAX_BACKEND_N < 1
#    error "Hey, Can not silent backends in this way!"
#endif

#define KPRINTF_BUFFER_SIZE (512)
#if KPRINTF_BUFFER_SIZE < 16
#    error "Setting KPRINTF_BUFFER_SIZE less then 16 is never a good idea..."
#endif

/**
 * @brief Defines in the @file kprintf_backends.h
 *
 */
#define KPRINTF_DEFAULT_BACKEND (KLOG_BACKEND_SERIAL)

#define KPRINTF_DEFAULT_FILTERED_LOGLEVEL (KLOG_LEVEL_TRACE)

/**
 * @brief Size of the format buffer for number conversion
 *
 * Used by the shared formatting module for converting numbers to strings.
 * Must be large enough for:
 * - 64-bit numbers in decimal (up to 20 digits + sign)
 * - 64-bit numbers in hex (16 digits + "0x" prefix)
 */
#define KPRINTF_FORMAT_BUFFER_SIZE (32)