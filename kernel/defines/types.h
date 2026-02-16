/**
 * @file types.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief types are types because they are types!
 * @version 0.1
 * @date 2026-02-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
#    if __STDC_HOSTED__ == 0
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef int64_t intptr_t;
typedef uint64_t uintptr_t;
typedef uint64_t size_t;
typedef int64_t ptrdiff_t;
// ============================================================================
// Type Limits (Min/Max Values)
// ============================================================================
#        define INT8_MIN ((int8_t)0x80)
#        define INT8_MAX ((int8_t)0x7F)
#        define UINT8_MAX ((uint8_t)0xFF)

#        define INT16_MIN ((int16_t)0x8000)
#        define INT16_MAX ((int16_t)0x7FFF)
#        define UINT16_MAX ((uint16_t)0xFFFF)

#        define INT32_MIN ((int32_t)0x80000000)
#        define INT32_MAX ((int32_t)0x7FFFFFFF)
#        define UINT32_MAX ((uint32_t)0xFFFFFFFF)

#        define INT64_MIN ((int64_t)0x8000000000000000LL)
#        define INT64_MAX ((int64_t)0x7FFFFFFFFFFFFFFFLL)
#        define UINT64_MAX ((uint64_t)0xFFFFFFFFFFFFFFFFULL)
#    else
#        include <stddef.h>
#        include <stdint.h>
#    endif
#else
#    if __STDC_HOSTED__ == 0
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef int32_t intptr_t;
typedef uint32_t uintptr_t;
typedef uint32_t size_t;
typedef int32_t ptrdiff_t;
// ============================================================================
// Type Limits (Min/Max Values)
// ============================================================================
#        define INT8_MIN ((int8_t)0x80)
#        define INT8_MAX ((int8_t)0x7F)
#        define UINT8_MAX ((uint8_t)0xFF)

#        define INT16_MIN ((int16_t)0x8000)
#        define INT16_MAX ((int16_t)0x7FFF)
#        define UINT16_MAX ((uint16_t)0xFFFF)

#        define INT32_MIN ((int32_t)0x80000000)
#        define INT32_MAX ((int32_t)0x7FFFFFFF)
#        define UINT32_MAX ((uint32_t)0xFFFFFFFF)

#        define INT64_MIN ((int64_t)0x8000000000000000LL)
#        define INT64_MAX ((int64_t)0x7FFFFFFFFFFFFFFFLL)
#        define UINT64_MAX ((uint64_t)0xFFFFFFFFFFFFFFFFULL)
#    else
#        include <stddef.h>
#        include <stdint.h>
#    endif
#endif

// ============================================================================
// Useful Macros & Constants
// ============================================================================
#ifndef NULL
#    define NULL ((void*)0)
#endif

// Alignment macro
#define ALIGN(x) __attribute__((aligned(x)))

// Packed structure attribute
#define PACKED __attribute__((packed))

// ============================================================================
// Common Utility Types
// ============================================================================
typedef uint32_t physical_addr_t; // Physical memory address
typedef uintptr_t virtual_addr_t; // Virtual memory address
