// CCOS Kernel Main Entry
// C code entry point called from kernel_entry.asm
#include "defines/types.h"
#include "driver/vga/vga.h"
#include "driver/vga/vga_example.h"

// Simple kernel with more code to test multi-sector loading
void kernel_main(void) {
    volatile char* video = (char*)0xB8000;
    const char* msg = "CCOS KERNEL RUNNING!";

    // Print message to screen (simple version)
    volatile int i = 0;
    while (msg[i] != '\0' && i < 80) {
        video[i * 2] = msg[i];
        video[i * 2 + 1] = 0x0F; // White on black
        i++;
    }

    system_vga_init();
    // vga_example_show();
    // Halt
    while (1) {
        __asm__ volatile("hlt");
    }
}