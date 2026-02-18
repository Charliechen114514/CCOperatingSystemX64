/* ==============================================================================
 * CCOS User Library - x86_64 System Call Implementation
 * ==============================================================================
 *
 * x86_64 architecture system call invocation using the 'syscall' instruction.
 *
 * ==============================================================================
 */

#include "syscall.h"

/* ============================================================================
 * System Call Wrapper Functions
 * ============================================================================ */

int64_t _syscall0(long num) {
    int64_t ret;
    __asm__ volatile(
        "syscall;"
        : "=a"(ret)
        : "a"(num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

int64_t _syscall1(long num, long arg1) {
    int64_t ret;
    __asm__ volatile(
        "syscall;"
        : "=a"(ret)
        : "a"(num), "D"(arg1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

int64_t _syscall2(long num, long arg1, long arg2) {
    int64_t ret;
    __asm__ volatile(
        "syscall;"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

int64_t _syscall3(long num, long arg1, long arg2, long arg3) {
    int64_t ret;
    __asm__ volatile(
        "syscall;"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

int64_t _syscall6(long num, long arg1, long arg2, long arg3,
                  long arg4, long arg5, long arg6) {
    int64_t ret;
    long r10 = arg4;
    long r8 = arg5;
    long r9 = arg6;
    __asm__ volatile(
        "syscall;"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}
