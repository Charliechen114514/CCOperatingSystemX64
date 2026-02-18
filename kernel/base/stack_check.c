/* ==============================================================================
 * CCOS - Stack Protector Support
 * ==============================================================================
 *
 * This module provides the stack protector failure handler required by
 * GCC's stack smashing protection (-fstack-protector) feature.
 *
 * When stack protection is enabled, GCC inserts a "canary" value on the
 * stack and checks it before function return. If the canary has been
 * modified (indicating a buffer overflow), GCC calls __stack_chk_fail.
 *
 * In a freestanding environment like an OS kernel, we must provide our
 * own implementation since the standard library is not available.
 * ==============================================================================
 */

#include "defines/types.h"

/**
 * __stack_chk_fail - Stack protector failure handler
 *
 * Called when stack protection detects corruption. This happens when
 * the stack canary value is modified, indicating a buffer overflow or
 * memory corruption.
 *
 * This implementation:
 * 1. Disables interrupts immediately
 * 2. Writes an error message directly to VGA buffer
 * 3. Halts the system (since continuing would be unsafe)
 *
 * Note: We write directly to VGA to avoid dependencies on the logging
 * system which may not be available during early boot or in test
 * environments.
 */
__attribute__((noreturn))
void __stack_chk_fail(void) {
    /* Disable interrupts first */
    __asm__ volatile("cli");

    /* Write error message directly to VGA buffer */
    /* VGA text mode buffer is at physical address 0xB8000 */
    /* Each character is 2 bytes: (char << 8) | attribute */
    /* Attribute 0x4C = red (0x4) on black (0x0) */
    const char* msg = "STACK CORRUPTION DETECTED - HALTING";
    volatile uint16_t* vga_buffer = (volatile uint16_t*)0xB8000;

    /* Write message to VGA */
    for (int i = 0; msg[i] != '\0'; i++) {
        vga_buffer[i] = (uint16_t)msg[i] | 0x4C00;  /* red on black */
    }

    /* Halt the system - infinite loop with hlt instruction */
    while (1) {
        __asm__ volatile("hlt");
    }
}
