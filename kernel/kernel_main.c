// CCOS Kernel Main Entry
// C code entry point called from kernel_entry.asm
#include "assert/assert.h"
#include "defines/types.h"
#include "driver/serial/serial.h"
#include "driver/vga/vga.h"
#include "kernel_init.h"
#include "klogs/kprintf.h"
// 孩子们千万不能在前面这里定义函数，小心被肘飞啊
// Simple kernel with more code to test multi-sector loading
void kernel_main(void) {
    kernel_init();
    klog_info("===Kernel Stage Reach End===");
    while (1) {
        __asm__ volatile("hlt");
    }
}
