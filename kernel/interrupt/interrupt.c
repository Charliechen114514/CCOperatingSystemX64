/**
 * @file interrupt.c
 * @brief Interrupt subsystem implementation
 */

#include "interrupt.h"
#include "driver/pic/pic.h"
#include "idt.h"
#include "idt_constants.h"
#include "klogs/kprintf.h"

/* ============================================================================
 * Interrupt Subsystem Initialization
 * ============================================================================ */

/**
 * @brief Internal function to enable all IRQ lines
 *
 * This is called by interrupt_finalize() after all handlers
 * have been registered.
 */
static void interrupt_enable_all_irqs(void) {
    for (int i = 0; i < 16; i++) {
        pic_enable_irq(i);
    }
    klog_trace("All IRQs (0-15) enabled\n");
}

void interrupt_init(void) {
    klog_trace("Initializing interrupt subsystem...\n");

    // Step 1: Initialize and remap the PIC
    // Remap IRQs 0-15 to vectors 32-47 to avoid CPU exceptions (0-31)
    pic_init(0x20, 0x28); // offset1=32, offset2=40
    klog_trace("PIC initialized: IRQs remapped to vectors 32-47\n");

    // Step 2: Disable all IRQs first
    pic_disable_all();

    // Step 3: Initialize the IDT
    idt_init();
    klog_trace("IDT initialized\n");

    // Note: Interrupts are NOT enabled yet.
    // All interrupt-dependent devices should be initialized first,
    // then call interrupt_finalize() to enable interrupts.
}

/**
 * @brief Finalize interrupt initialization and enable interrupts
 *
 * This function should be called AFTER all interrupt handlers
 * have been registered (e.g., timer_init(), keyboard_init(), etc.)
 */
void interrupt_finalize(void) {
    // Enable all IRQ lines
    interrupt_enable_all_irqs();

    // Enable CPU interrupts
    interrupt_enable();
    klog_info("Interrupts enabled\n");
}
