/**
 * @file syscall_table.c
 * @brief System call handler table
 *
 * This file contains the implementation of all system calls.
 * New syscalls should be added here.
 */

#include "syscall.h"
#include "klogs/kprintf.h"
#include "process/process.h"

/* Forward declaration for registration function */
extern int syscall_register_handler(uint64_t number, syscall_handler_fn handler, const char* name);

/* ============================================================================
 * Process Management Syscalls
 * ============================================================================ */

/**
 * @brief Fork - create a new process
 */
static int64_t sys_fork(syscall_frame_t* frame) {
    (void)frame;
    int32_t result = proc_fork();
    return (int64_t)result;
}

/**
 * @brief Exit current process
 */
static int64_t sys_exit(syscall_frame_t* frame) {
    int exit_code = (int)frame->arg0;
    proc_exit(exit_code);
    __builtin_unreachable();  /* proc_exit never returns */
}

/**
 * @brief Wait for a child process to exit
 */
static int64_t sys_wait4(syscall_frame_t* frame) {
    int32_t pid = (int32_t)frame->arg0;
    int* wstatus = (int*)frame->arg1;
    int options = (int)frame->arg2;
    int32_t result = proc_wait4(pid, wstatus, options);
    return (int64_t)result;
}

/**
 * @brief Get process ID
 */
static int64_t sys_getpid(syscall_frame_t* frame) {
    (void)frame;
    pcb_t* current = proc_current();
    return current ? (int64_t)current->pid : -1;
}

/**
 * @brief Get parent process ID
 */
static int64_t sys_getppid(syscall_frame_t* frame) {
    (void)frame;
    pcb_t* current = proc_current();
    if (current && current->parent) {
        return (int64_t)current->parent->pid;
    }
    return 0;
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
    syscall_register_handler(SYS_FORK, sys_fork, "fork");
    syscall_register_handler(SYS_EXIT, sys_exit, "exit");
    syscall_register_handler(SYS_WAIT4, sys_wait4, "wait4");
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
