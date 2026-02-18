/* ==============================================================================
 * CCOS User Library - System Call Interface
 * ==============================================================================
 *
 * Internal system call macros for architecture-specific implementations.
 *
 * ==============================================================================
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * System Call Numbers (must match kernel/syscall/syscall_numbers.h)
 * ============================================================================ */

#define SYS_EXIT        0
#define SYS_FORK        2
#define SYS_READ        3
#define SYS_WRITE       13
#define SYS_OPEN        14
#define SYS_CLOSE       15
#define SYS_WAIT4       16
#define SYS_GETPID      4
#define SYS_GETPPID     5
#define SYS_BRK         20
#define SYS_MMAP        21
#define SYS_MUNMAP      22
#define SYS_UNAME       30

/* ============================================================================
 * System Call Result Codes
 * ============================================================================ */

#define SYS_OK          0
#define SYS_ERR_INVAL   -22
#define SYS_ERR_NFILE   -9
#define SYS_ERR_NOENT   -2
#define SYS_ERR_NOTIMPL -38
#define SYS_ERR_IO      -5

/* ============================================================================
 * Architecture-specific system call invocation
 * ============================================================================ */

/* Generic system call macros (implemented per-architecture) */

/* 0-argument syscall */
extern int64_t _syscall0(long num);

/* 1-argument syscall */
extern int64_t _syscall1(long num, long arg1);

/* 2-argument syscall */
extern int64_t _syscall2(long num, long arg1, long arg2);

/* 3-argument syscall */
extern int64_t _syscall3(long num, long arg1, long arg2, long arg3);

/* 6-argument syscall (for mmap) */
extern int64_t _syscall6(long num, long arg1, long arg2, long arg3,
                         long arg4, long arg5, long arg6);

#ifdef __cplusplus
}
#endif
