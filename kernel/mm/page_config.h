/* ==============================================================================
 * CCOS - Page Configuration
 * ==============================================================================
 * Shared page-related configuration constants for the memory management subsystem.
 * These definitions are used by both pframe and vmm modules.
 * ==============================================================================
 */

#pragma once

/* ==============================================================================
 * Basic Page Size Constants (x86_64)
 * ============================================================================== */

#define PAGE_SHIFT       12      /* log2(PAGE_SIZE) */
#define PAGE_SIZE        (1ULL << PAGE_SHIFT)   /* 4096 bytes */

/* ==============================================================================
 * Huge Page Sizes
 * ============================================================================== */

#define PAGE_SIZE_2MB    (2ULL * 1024 * 1024)   /* 2097152 bytes */
#define PAGE_SIZE_1GB    (1ULL * 1024 * 1024 * 1024)  /* 1073741824 bytes */
#define PAGE_SHIFT_2MB   21
#define PAGE_SHIFT_1GB   30
