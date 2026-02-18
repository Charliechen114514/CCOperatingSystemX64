#pragma once

/* ==============================================================================
 * Constants
 * ============================================================================== */

#define HEAP_ALIGN 16      /* 16-byte alignment */
#define HEAP_MIN_ALLOC 16  /* Minimum allocation size */
#define HEAP_MIN_BLOCK 32  /* Minimum block size (header + data) */
#define HEAP_INIT_PAGES 16 /* Initial heap size (64KB) */
