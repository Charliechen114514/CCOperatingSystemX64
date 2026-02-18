/* ==============================================================================
 * User Mode Demo Program - uname syscall test
 * ==============================================================================
 *
 * This is a simple user mode program that:
 * 1. Calls uname() syscall to get system information
 * 2. Prints the uname structure using write() syscall
 * 3. Exits with exit() syscall
 *
 * NOTE: This program is compiled separately and loaded by the kernel.
 * It must NOT link against any library functions to avoid symbol conflicts.
 *
 * ==============================================================================
 */

/* System call numbers (must match kernel) */
#define SYS_EXIT        0
#define SYS_WRITE       13
#define SYS_UNAME       30

/* File descriptors */
#define STDOUT_FILENO   1

/* utsname structure */
struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

/* ==============================================================================
 * System Call Wrappers (inline to avoid external dependencies)
 * ============================================================================== */

static inline long syscall0(long num) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(num) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall1(long num, long arg1) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(num), "D"(arg1) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall3(long num, long arg1, long arg2, long arg3) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
                      : "rcx", "r11", "memory");
    return ret;
}

static void exit(int status) {
    syscall1(SYS_EXIT, status);
    __builtin_unreachable();
}

static long write(int fd, const void* buf, long count) {
    return syscall3(SYS_WRITE, fd, (long)buf, count);
}

static int uname(struct utsname* buf) {
    return (int)syscall1(SYS_UNAME, (long)buf);
}

/* ==============================================================================
 * Simple string length (inline to avoid conflicts)
 * ============================================================================== */

static long str_len(const char* s) {
    long len = 0;
    while (s[len]) len++;
    return len;
}

/* ==============================================================================
 * Main Program
 * ============================================================================== */

void _start(void) {
    struct utsname uts;
    char newline = '\n';

    /* Call uname */
    int ret = uname(&uts);
    if (ret != 0) {
        /* Error: write error message */
        const char* err = "uname() failed!\n";
        write(STDOUT_FILENO, err, 14);
        exit(1);
    }

    /* Print sysname */
    const char* sysname_label = "sysname: ";
    write(STDOUT_FILENO, sysname_label, 9);
    long len = str_len(uts.sysname);
    write(STDOUT_FILENO, uts.sysname, len);
    write(STDOUT_FILENO, &newline, 1);

    /* Print nodename */
    const char* nodename_label = "nodename: ";
    write(STDOUT_FILENO, nodename_label, 10);
    len = str_len(uts.nodename);
    write(STDOUT_FILENO, uts.nodename, len);
    write(STDOUT_FILENO, &newline, 1);

    /* Print release */
    const char* release_label = "release: ";
    write(STDOUT_FILENO, release_label, 9);
    len = str_len(uts.release);
    write(STDOUT_FILENO, uts.release, len);
    write(STDOUT_FILENO, &newline, 1);

    /* Print version */
    const char* version_label = "version: ";
    write(STDOUT_FILENO, version_label, 9);
    len = str_len(uts.version);
    write(STDOUT_FILENO, uts.version, len);
    write(STDOUT_FILENO, &newline, 1);

    /* Print machine */
    const char* machine_label = "machine: ";
    write(STDOUT_FILENO, machine_label, 9);
    len = str_len(uts.machine);
    write(STDOUT_FILENO, uts.machine, len);
    write(STDOUT_FILENO, &newline, 1);

    /* Print domainname */
    const char* domainname_label = "domainname: ";
    write(STDOUT_FILENO, domainname_label, 13);
    len = str_len(uts.domainname);
    write(STDOUT_FILENO, uts.domainname, len);
    write(STDOUT_FILENO, &newline, 1);

    /* Exit successfully */
    exit(0);
}
