// CCOS Kernel Main Entry
// C code entry point called from kernel_entry.asm
#include "defines/types.h"
#include "driver/serial/serial.h"
#include "driver/timer/timer.h"
#include "driver/vga/vga.h"
#include "interrupt/interrupt.h"
#include "kernel_init.h"
#include "klogs/kprintf.h"
#include "shell/backends/serial_shell.h"
#include "shell/backends/vga_shell.h"
// 孩子们千万不能在前面这里定义函数，小心被肘飞啊

// Forward declaration for demo controller (always available, checks internally)
void run_possible_demos(void);

// Simple kernel with more code to test multi-sector loading
void kernel_main(void) {
    kernel_init();
    klog_info("===Kernel Stage Init Reach End===\n");
    // Run enabled demos after system initialization is complete
    run_possible_demos();

    // serial_shell_run();
    // vga_shell_run();
    while (1) {
        __asm__ volatile("hlt");
    }
}
