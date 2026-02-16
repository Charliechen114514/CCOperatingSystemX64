// CCOS Kernel Main Entry
// C code entry point called from kernel_entry.asm
#include "assert/assert.h"
#include "defines/types.h"
#include "driver/serial/serial.h"
#include "driver/vga/vga.h"
#include "kernel_init.h"

// 孩子们千万不能在前面这里定义函数，小心被肘飞啊
// Simple kernel with more code to test multi-sector loading
void kernel_main(void) {
    kernel_init();
    CCOS_VGA* vga = vga_instance();
    vga_set_cursor(vga, 0, 23);
    vga_print_string(vga, "Very long string that will auto-scrollVery long string that will "
                          "auto-scrollVery long string that will auto-scrollVery long string that "
                          "will auto-scrollVery long string that will auto-scrollVery long string "
                          "that will auto-scrollVery long string that will auto-scroll");
    vga_print_string(vga, "Very long string that will auto-scrollVery long string that will "
                          "auto-scrollVery long string that will auto-scrollVery long string that "
                          "will auto-scrollVery long string that will auto-scrollVery long string "
                          "that will auto-scrollVery long string that will auto-scroll");

    // CCOS_ASSERT(0 == 1);
    // Halt

    sync_serial_puts("\033[0;32m===Kernel Stage Reach End===\n\033[0m");
    while (1) {
        __asm__ volatile("hlt");
    }
}
