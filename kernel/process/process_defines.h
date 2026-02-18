/* ==============================================================================
 * CCOS - Process Type Definitions
 * ==============================================================================
 * Basic type definitions for process management.
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"

/* ==============================================================================
 * Process Types
 * ============================================================================== */

/**
 * @brief Process ID type
 */
typedef int32_t pid_t;

/* ==============================================================================
 * Signal Numbers
 * ==============================================================================
 * POSIX-compatible signal numbers for process termination.
 */

#define SIGEXIT 0    /* Exit */
#define SIGHUP  1    /* Hangup */
#define SIGINT  2    /* Interrupt */
#define SIGQUIT 3    /* Quit */
#define SIGILL  4    /* Illegal instruction */
#define SIGTRAP 5    /* Trace trap */
#define SIGABRT 6    /* Abort */
#define SIGBUS  7    /* BUS error */
#define SIGFPE  8    /* Floating-point exception */
#define SIGKILL 9    /* Kill (cannot be caught/ignored) */
#define SIGSEGV 11   /* Segmentation violation */
