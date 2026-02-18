/* ==============================================================================
 * CCOS User Library - Standard Definitions
 * ==============================================================================
 *
 * Standard type definitions and macros.
 *
 * ==============================================================================
 */

#pragma once

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Standard Types
 * ============================================================================ */

typedef intptr_t ssize_t;
typedef size_t size_t;

#ifndef NULL
#define NULL    ((void*)0)
#endif

#define offsetof(type, member) __builtin_offsetof(type, member)

#ifdef __cplusplus
}
#endif
