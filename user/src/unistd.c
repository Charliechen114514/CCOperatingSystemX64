/* ==============================================================================
 * CCOS User Library - System Call Implementation
 * ==============================================================================
 *
 * Implementation of POSIX-compatible system call wrappers.
 *
 * ==============================================================================
 */

#include "unistd.h"
#include "syscall.h"

/* ============================================================================
 * Process Management
 * ============================================================================ */

void exit(int status) {
    _syscall1(SYS_EXIT, status);
    __builtin_unreachable();
}

int fork(void) {
    return (int)_syscall0(SYS_FORK);
}

int getpid(void) {
    return (int)_syscall0(SYS_GETPID);
}

int getppid(void) {
    return (int)_syscall0(SYS_GETPPID);
}

/* ============================================================================
 * File I/O
 * ============================================================================ */

ssize_t write(int fd, const void* buf, size_t count) {
    return (ssize_t)_syscall3(SYS_WRITE, fd, (long)buf, (long)count);
}

ssize_t read(int fd, void* buf, size_t count) {
    return (ssize_t)_syscall3(SYS_READ, fd, (long)buf, (long)count);
}

int close(int fd) {
    return (int)_syscall1(SYS_CLOSE, fd);
}

/* ============================================================================
 * Memory Management
 * ============================================================================ */

void* sbrk(intptr_t incr) {
    return (void*)_syscall1(SYS_BRK, incr);
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, size_t offset) {
    return (void*)_syscall6(SYS_MMAP, (long)addr, (long)length, (long)prot,
                           (long)flags, (long)fd, (long)offset);
}

int munmap(void* addr, size_t length) {
    return (int)_syscall2(SYS_MUNMAP, (long)addr, (long)length);
}

/* ============================================================================
 * System Information
 * ============================================================================ */

int uname(struct utsname* buf) {
    return (int)_syscall1(SYS_UNAME, (long)buf);
}
