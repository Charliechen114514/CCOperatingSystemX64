// CCOS Kernel Main Entry
// C code entry point called from kernel_entry.asm

#include "types.h"

// Kernel main function - called from assembly entry
void kernel_main(void) {
    // Simple VGA test - put 'C' on line 6 to prove C code is running
    volatile uint16_t *vga = (uint16_t *)0xB8000;
    vga[160 * 6] = 0x1F43;  // 'C' in white on blue

    // TODO: Initialize serial port for no-graphic debugging

    // Infinite loop - halt here
    while (1) {
        __asm__ volatile("hlt");
    }
}
