/**
 * @file syscall_numbers.h
 * @brief System call number definitions
 */

#pragma once

#include "defines/types.h"

/* ============================================================================
 * System Call Numbers
 * ============================================================================ */

/**
 * @brief System call numbers
 *
 * These are the values passed in RAX when invoking a syscall.
 * Range 0-255 is reserved for kernel-defined syscalls.
 */
typedef enum syscall_number {
    /* Process management */
    SYS_EXIT       = 0,    /* Exit current process */
    SYS_EXECVE     = 1,    /* Execute a program */
    SYS_FORK       = 2,    /* Create a new process */
    SYS_WAIT4      = 3,    /* Wait for process to change state */
    SYS_GETPID     = 4,    /* Get process ID */
    SYS_GETPPID    = 5,    /* Get parent process ID */

    /* File I/O */
    SYS_OPEN       = 10,   /* Open a file */
    SYS_CLOSE      = 11,   /* Close a file descriptor */
    SYS_READ       = 12,   /* Read from file descriptor */
    SYS_WRITE      = 13,   /* Write to file descriptor */
    SYS_LSEEK      = 14,   /* Reposition file offset */
    SYS_IOCTL      = 15,   /* Device-specific operations */

    /* Memory management */
    SYS_BRK        = 20,   /* Change data segment size */
    SYS_MMAP       = 21,   /* Map files or devices into memory */
    SYS_MUNMAP     = 22,   /* Unmap files or devices into memory */

    /* System information */
    SYS_UNAME      = 30,   /* Get system information */
    SYS_GETTIME    = 31,   /* Get system time */

    /* Debug/Testing (for early development) */
    SYS_DEBUG_LOG  = 100,  /* Debug logging syscall */
    SYS_TEST       = 101,  /* Test syscall */

    /* Maximum syscall number (for table size) */
    SYS_MAX        = 256,
} syscall_number_t;

/**
 * @brief System call return codes
 */
typedef enum syscall_result {
    SYS_OK           = 0,   /* Success */
    SYS_ERR_INVAL    = -1,  /* Invalid argument */
    SYS_ERR_PERM     = -2,  /* Permission denied */
    SYS_ERR_NOMEM    = -3,  /* Out of memory */
    SYS_ERR_NFILE    = -4,  /* File table overflow */
    SYS_ERR_NOENT    = -5,  /* No such file or directory */
    SYS_ERR_IO       = -6,  /* I/O error */
    SYS_ERR_NOTIMPL  = -7,  /* Not implemented */
} syscall_result_t;
