// CCOS Kernel Main Entry
// C code entry point called from kernel_entry.asm
#include "assert/assert.h"
#include "defines/types.h"
#include "driver/serial/serial.h"
#include "driver/vga/vga.h"
#include "interrupt/interrupt.h"
#include "kernel_init.h"
#include "klogs/kprintf.h"
// 孩子们千万不能在前面这里定义函数，小心被肘飞啊
// Simple kernel with more code to test multi-sector loading
void kernel_main(void) {
    kernel_init();
    klog_info("===Kernel Stage Reach End===");

    // Monitor timer ticks periodically
    uint64_t last_ticks = 0;
    while (1) {
        uint64_t current_ticks = timer_get_ticks();
        // Log every ~100 ticks (if timer is working at default rate)
        if (current_ticks > last_ticks + 99) {
            klog_info("[TIMER] Active! Total ticks: %lu\n", current_ticks);
            last_ticks = current_ticks;
        }
        __asm__ volatile("hlt");
    }
}
