// CCOS Kernel Main Entry
// C code entry point called from kernel_entry.asm
#include "defines/types.h"
#include "kernel_init.h"
// 孩子们千万不能在前面这里定义函数，小心被肘飞啊
// Simple kernel with more code to test multi-sector loading
void kernel_main(void) {
    kernel_init();

    // Halt
    while (1) {
        __asm__ volatile("hlt");
    }
}
