/**
 * @file syscall_table.c
 * @brief System call handler table
 *
 * This file contains the implementation of all system calls.
 * New syscalls should be added here.
 */

#include "base/memory.h"
#include "ccos_config.h"
#include "klogs/kprintf.h"
#include "mm/pframe/pframe.h"
#include "mm/vmm/page.h"
#include "mm/vmm/vmm.h"
#include "process/process.h"
#include "syscall.h"
#include "user/user.h"

/* File offset type */
typedef int64_t off_t;

/* utsname structure for uname syscall */
struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

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
    __builtin_unreachable(); /* proc_exit never returns */
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

    if (fd == 1 || fd == 2) { /* stdout or stderr */
        klog_info("[USER OUT] %.*s", (int)count, buf);
        return (int64_t)count;
    }

    return SYS_ERR_NFILE; /* Bad file descriptor */
}

/**
 * @brief Read from a file descriptor
 */
static int64_t sys_read(syscall_frame_t* frame) {
    int fd = (int)frame->arg0;
    char* buf = (char*)frame->arg1;
    size_t count = (size_t)frame->arg2;

    (void)buf; /* TODO: Use buffer for actual input */
    (void)count;

    /* For now, only support stdin (fd=0) - return 0 bytes (EOF) */
    if (fd == 0) {
        /* TODO: Implement actual keyboard input */
        return 0; /* EOF for now */
    }

    return SYS_ERR_NFILE; /* Bad file descriptor */
}

/**
 * @brief Open a file
 */
static int64_t sys_open(syscall_frame_t* frame) {
    const char* filename = (const char*)frame->arg1;
    int flags = (int)frame->arg2;
    (void)filename;
    (void)flags;

    /* For now, no filesystem - return error */
    return SYS_ERR_NOENT;
}

/**
 * @brief Close a file descriptor
 */
static int64_t sys_close(syscall_frame_t* frame) {
    int fd = (int)frame->arg0;
    (void)fd;
    /* Silently succeed for now */
    return SYS_OK;
}

/**
 * @brief Lseek - reposition file offset
 */
static int64_t sys_lseek(syscall_frame_t* frame) {
    int fd = (int)frame->arg0;
    off_t offset = (off_t)frame->arg1;
    int whence = (int)frame->arg2;

    (void)offset;
    (void)whence;

    if (fd == 0 || fd == 1 || fd == 2) {
        /* Can't seek on stdin/stdout/stderr */
        return SYS_ERR_INVAL;
    }

    return SYS_ERR_NFILE; /* Bad file descriptor */
}

/**
 * @brief Ioctl - device-specific operations
 */
static int64_t sys_ioctl(syscall_frame_t* frame) {
    (void)frame;
    return SYS_ERR_NOTIMPL;
}

/* ============================================================================
 * Memory Management Syscalls
 * ============================================================================ */

/**
 * @brief Change data segment size (brk)
 */
static int64_t sys_brk(syscall_frame_t* frame) {
    void* new_brk = (void*)frame->arg0;
    pcb_t* current = proc_current();

    if (!current) {
        return SYS_ERR_INVAL;
    }

    /* NULL argument queries current break */
    if (new_brk == NULL) {
        return (int64_t)current->mm.brk;
    }

    /* Call user_brk to handle the actual break change */
    virtual_addr_t result = user_brk(current, (virtual_addr_t)new_brk);
    return (int64_t)result;
}

/**
 * @brief mmap - Map files or devices into memory
 */
static int64_t sys_mmap(syscall_frame_t* frame) {
    virtual_addr_t addr = (virtual_addr_t)frame->arg0;
    size_t length = (size_t)frame->arg1;
    int prot = (int)frame->arg2;
    int flags = (int)frame->arg3;
    int fd = (int)frame->arg4;
    size_t offset = (size_t)frame->arg5;

    (void)fd;     /* No filesystem yet */
    (void)offset; /* No filesystem yet */

    pcb_t* current = proc_current();
    if (!current) {
        return SYS_ERR_INVAL;
    }

    /* Validate flags - only support anonymous private mappings for now */
    if (!(flags & 0x20)) {      /* MAP_ANONYMOUS = 0x20 */
        return SYS_ERR_NOTIMPL; /* Only anonymous mappings */
    }

    /* Call user_mmap to handle the actual mapping */
    virtual_addr_t result = user_mmap(current, addr, length, prot, flags, fd, offset);
    return (int64_t)result;
}

/**
 * @brief munmap - Unmap memory
 */
static int64_t sys_munmap(syscall_frame_t* frame) {
    virtual_addr_t addr = (virtual_addr_t)frame->arg0;
    size_t length = (size_t)frame->arg1;

    pcb_t* current = proc_current();
    if (!current) {
        return SYS_ERR_INVAL;
    }

    /* Call user_munmap to handle the actual unmapping */
    int result = user_munmap(current, addr, length);
    return (int64_t)result;
}

/* ============================================================================
 * System Information Syscalls
 * ============================================================================ */

/**
 * @brief Get system information (uname)
 */
static int64_t sys_uname(syscall_frame_t* frame) {
    struct utsname* buf = (struct utsname*)frame->arg0;

    pcb_t* current = proc_current();
    if (!current || !user_validate_pointer(buf, sizeof(struct utsname), true)) {
        return SYS_ERR_INVAL;
    }

    /* Create kernel info structure */
    struct utsname kernel_info = {
        .sysname = "CCOS",
        .nodename = "localhost",
        .release = CCOS_VERSION,
        .version = "CCOS x86_64 v" CCOS_VERSION,
        .machine = "x86_64",
        .domainname = "(none current)",
    };

    /* Copy to user space */
    int64_t result = user_copy_to_user(buf, &kernel_info, sizeof(struct utsname));
    if (result < 0) {
        return SYS_ERR_IO;
    }

    return SYS_OK;
}

/* ============================================================================
 * Thread Management Syscalls
 * ============================================================================ */

/**
 * @brief Create a new thread
 *
 * Arguments:
 *   arg0 - Thread entry point (function pointer)
 *   arg1 - Argument to pass to thread function
 *   arg2 - User stack address (0 to auto-allocate)
 *   arg3 - Stack size in bytes
 *
 * Returns:
 *   New thread ID on success, negative error code on failure
 */
static int64_t sys_thread_create(syscall_frame_t* frame) {
    virtual_addr_t entry = frame->arg0;
    virtual_addr_t arg = frame->arg1;
    virtual_addr_t stack = frame->arg2;
    size_t stack_size = (size_t)frame->arg3;

    pcb_t* current = proc_current();
    if (!current) {
        return SYS_ERR_INVAL;
    }

    /* Validate entry point */
    if (entry == 0) {
        return SYS_ERR_INVAL;
    }

    int32_t tid = proc_create_user_thread(current, entry, arg, stack, stack_size);
    if (tid < 0) {
        return SYS_ERR_NOMEM;  /* Most likely error */
    }

    return (int64_t)tid;
}

/**
 * @brief Exit current thread
 *
 * Arguments:
 *   arg0 - Return value
 *
 * Does not return.
 */
static int64_t sys_thread_exit(syscall_frame_t* frame) {
    void* retval = (void*)frame->arg0;
    proc_thread_exit(retval);
    __builtin_unreachable();  /* proc_thread_exit never returns */
}

/**
 * @brief Wait for a thread to exit
 *
 * Arguments:
 *   arg0 - Thread ID to wait for
 *   arg1 - Pointer to store return value (can be NULL)
 *
 * Returns:
 *   0 on success, negative error code on failure
 */
static int64_t sys_thread_join(syscall_frame_t* frame) {
    int32_t tid = (int32_t)frame->arg0;
    void** retval = (void**)frame->arg1;

    pcb_t* current = proc_current();
    if (!current) {
        return SYS_ERR_INVAL;
    }

    /* Validate retval pointer if provided */
    if (retval != NULL) {
        if (!user_validate_pointer(retval, sizeof(void*), true)) {
            return SYS_ERR_INVAL;
        }
    }

    int32_t result = proc_thread_join(tid, retval);

    if (result < 0) {
        return SYS_ERR_INVAL;
    }

    /* Copy return value to user space if provided and thread has returned */
    if (retval != NULL && result == 0) {
        /* Return value is already copied by proc_thread_join */
    }

    return SYS_OK;
}

/**
 * @brief Detach a thread
 *
 * Arguments:
 *   arg0 - Thread ID to detach
 *
 * Returns:
 *   0 on success, negative error code on failure
 */
static int64_t sys_thread_detach(syscall_frame_t* frame) {
    int32_t tid = (int32_t)frame->arg0;

    pcb_t* current = proc_current();
    if (!current) {
        return SYS_ERR_INVAL;
    }

    int32_t result = proc_thread_detach(tid);

    if (result < 0) {
        return SYS_ERR_INVAL;
    }

    return SYS_OK;
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

    /* Thread management */
    syscall_register_handler(SYS_THREAD_CREATE, sys_thread_create, "thread_create");
    syscall_register_handler(SYS_THREAD_EXIT, sys_thread_exit, "thread_exit");
    syscall_register_handler(SYS_THREAD_JOIN, sys_thread_join, "thread_join");
    syscall_register_handler(SYS_THREAD_DETACH, sys_thread_detach, "thread_detach");

    /* File I/O */
    syscall_register_handler(SYS_WRITE, sys_write, "write");
    syscall_register_handler(SYS_READ, sys_read, "read");
    syscall_register_handler(SYS_OPEN, sys_open, "open");
    syscall_register_handler(SYS_CLOSE, sys_close, "close");
    syscall_register_handler(SYS_LSEEK, sys_lseek, "lseek");
    syscall_register_handler(SYS_IOCTL, sys_ioctl, "ioctl");

    /* Memory management */
    syscall_register_handler(SYS_BRK, sys_brk, "brk");
    syscall_register_handler(SYS_MMAP, sys_mmap, "mmap");
    syscall_register_handler(SYS_MUNMAP, sys_munmap, "munmap");

    /* System information */
    syscall_register_handler(SYS_UNAME, sys_uname, "uname");
}
