/**
 * @file syscall_table.c
 * @brief System call handler table
 *
 * This file contains the implementation of all system calls.
 * New syscalls should be added here.
 */

#include "syscall.h"
#include "klogs/kprintf.h"

/* Forward declaration for registration function */
extern int syscall_register_handler(uint64_t number, syscall_handler_fn handler, const char* name);

/* ============================================================================
 * Process Management Syscalls
 * ============================================================================ */

/**
 * @brief Exit current process
 */
static int64_t sys_exit(syscall_frame_t* frame) {
    int exit_code = (int)frame->arg0;
    klog_info("[SYSCALL] exit(%d)\n", exit_code);
    /* TODO: Implement process termination */
    return SYS_OK;
}

/**
 * @brief Get process ID
 */
static int64_t sys_getpid(syscall_frame_t* frame) {
    (void)frame;
    /* TODO: Return actual process ID */
    return 1;  /* Return dummy PID for now */
}

/**
 * @brief Get parent process ID
 */
static int64_t sys_getppid(syscall_frame_t* frame) {
    (void)frame;
    /* TODO: Return actual parent process ID */
    return 0;  /* Return dummy PPID for now */
}

/* ============================================================================
 * File I/O Syscalls
 * ============================================================================ */

/**
 * @brief Write to a file descriptor
 */
static int64_t sys_write(syscall_frame_t* frame) {
    int fd = (int)frame->arg0;
    const char* buf = (const char*)frame->arg1;
    size_t count = (size_t)frame->arg2;

    if (fd == 1 || fd == 2) {  /* stdout or stderr */
        klog_info("[USER OUT] %.*s", (int)count, buf);
        return (int64_t)count;
    }

    return SYS_ERR_NFILE;  /* Bad file descriptor */
}

/**
 * @brief Read from a file descriptor
 */
static int64_t sys_read(syscall_frame_t* frame) {
    (void)frame;
    /* TODO: Implement read */
    return SYS_ERR_NOTIMPL;
}

/**
 * @brief Open a file
 */
static int64_t sys_open(syscall_frame_t* frame) {
    (void)frame;
    /* TODO: Implement open */
    return SYS_ERR_NOTIMPL;
}

/**
 * @brief Close a file descriptor
 */
static int64_t sys_close(syscall_frame_t* frame) {
    (void)frame;
    /* TODO: Implement close */
    return SYS_OK;  /* Silently succeed for now */
}

/* ============================================================================
 * Memory Management Syscalls
 * ============================================================================ */

/**
 * @brief Change data segment size (brk)
 */
static int64_t sys_brk(syscall_frame_t* frame) {
    void* new_brk = (void*)frame->arg0;
    (void)new_brk;
    /* TODO: Implement brk */
    return SYS_ERR_NOTIMPL;
}

/* ============================================================================
 * System Information Syscalls
 * ============================================================================ */

/**
 * @brief Get system information (uname)
 */
static int64_t sys_uname(syscall_frame_t* frame) {
    (void)frame;
    /* TODO: Implement uname */
    return SYS_ERR_NOTIMPL;
}

/* ============================================================================
 * Initialization Function
 * ============================================================================ */

/**
 * @brief Register all system call handlers
 *
 * This function is called from syscall_init() to register
 * all system call handlers defined in this file.
 */
void syscall_register_all(void) {
    /* Process management */
    syscall_register_handler(SYS_EXIT, sys_exit, "exit");
    syscall_register_handler(SYS_GETPID, sys_getpid, "getpid");
    syscall_register_handler(SYS_GETPPID, sys_getppid, "getppid");

    /* File I/O */
    syscall_register_handler(SYS_WRITE, sys_write, "write");
    syscall_register_handler(SYS_READ, sys_read, "read");
    syscall_register_handler(SYS_OPEN, sys_open, "open");
    syscall_register_handler(SYS_CLOSE, sys_close, "close");

    /* Memory management */
    syscall_register_handler(SYS_BRK, sys_brk, "brk");

    /* System information */
    syscall_register_handler(SYS_UNAME, sys_uname, "uname");
}
