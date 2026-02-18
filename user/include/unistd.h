/* ==============================================================================
 * CCOS User Library - System Call Interface
 * ==============================================================================
 *
 * POSIX-compatible system call wrappers for user programs.
 *
 * ==============================================================================
 */

#pragma once

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * utsname structure (for uname syscall)
 * ============================================================================ */

struct utsname {
    char sysname[65];        /* Operating system name */
    char nodename[65];       /* Network node hostname */
    char release[65];        /* OS release */
    char version[65];        /* OS version */
    char machine[65];        /* Hardware identifier */
    char domainname[65];     /* NIS/YP domain name */
};

/* ============================================================================
 * Process Management
 * ============================================================================ */

/**
 * @brief Exit the current process
 * @param status Exit code (0 = success, non-zero = error)
 * @note This function never returns
 */
void exit(int status) __attribute__((noreturn));

/**
 * @brief Fork the current process
 * @return 0 to child process, PID of child to parent, negative on error
 */
int fork(void);

/**
 * @brief Get current process ID
 * @return Process ID
 */
int getpid(void);

/**
 * @brief Get parent process ID
 * @return Parent process ID
 */
int getppid(void);

/* ============================================================================
 * File I/O
 * ============================================================================ */

/**
 * @brief Write to a file descriptor
 * @param fd File descriptor (1 = stdout, 2 = stderr)
 * @param buf Buffer to write
 * @param count Number of bytes to write
 * @return Number of bytes written, or negative on error
 */
ssize_t write(int fd, const void* buf, size_t count);

/**
 * @brief Read from a file descriptor
 * @param fd File descriptor (0 = stdin)
 * @param buf Buffer to read into
 * @param count Maximum number of bytes to read
 * @return Number of bytes read, or negative on error
 */
ssize_t read(int fd, void* buf, size_t count);

/**
 * @brief Close a file descriptor
 * @param fd File descriptor to close
 * @return 0 on success, negative on error
 */
int close(int fd);

/* ============================================================================
 * Memory Management
 * ============================================================================ */

/**
 * @brief Change program break (heap management)
 * @param incr Increment to add to program break (can be negative)
 * @return Previous program break, or (void*)-1 on error
 */
void* sbrk(intptr_t incr);

/**
 * @brief Map files or devices into memory
 * @param addr Suggested address (NULL for any)
 * @param length Length of mapping
 * @param prot Protection flags (PROT_READ, PROT_WRITE, PROT_EXEC)
 * @param flags Mapping flags (MAP_PRIVATE, MAP_SHARED, MAP_ANONYMOUS)
 * @param fd File descriptor (-1 for anonymous)
 * @param offset File offset
 * @return Mapped address, or NULL on error
 */
void* mmap(void* addr, size_t length, int prot, int flags, int fd, size_t offset);

/**
 * @brief Unmap memory
 * @param addr Address to unmap
 * @param length Length of region
 * @return 0 on success, negative on error
 */
int munmap(void* addr, size_t length);

/* Protection flags for mmap */
#define PROT_READ      0x1
#define PROT_WRITE     0x2
#define PROT_EXEC      0x4

/* Mapping flags for mmap */
#define MAP_SHARED     0x01
#define MAP_PRIVATE    0x02
#define MAP_ANONYMOUS  0x20
#define MAP_FIXED      0x10

/* ============================================================================
 * System Information
 * ============================================================================ */

/**
 * @brief Get system information
 * @param buf Pointer to utsname structure to fill
 * @return 0 on success, negative on error
 */
int uname(struct utsname* buf);

#ifdef __cplusplus
}
#endif
