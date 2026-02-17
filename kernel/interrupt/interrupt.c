/**
 * @file interrupt.c
 * @brief Interrupt subsystem implementation
 */

#include "interrupt.h"
#include "idt.h"
#include "driver/pic/pic.h"
#include "klogs/kprintf.h"

// Timer tick counter
static volatile uint64_t timer_ticks = 0;

/* ============================================================================
 * Timer Interrupt Handler
 * ============================================================================ */

void timer_handler(interrupt_frame_t* frame, uint64_t error_code) {
    (void)frame;
    (void)error_code;

    timer_ticks++;

    // Send EOI to acknowledge the interrupt
    pic_send_eoi(0);
}

/* ============================================================================
 * Interrupt Subsystem Initialization
 * ============================================================================ */

void interrupt_init(void) {
    klog_info("Initializing interrupt subsystem...\n");

    // Step 1: Initialize and remap the PIC
    // Remap IRQs 0-15 to vectors 32-47 to avoid CPU exceptions (0-31)
    pic_init(0x20, 0x28);  // offset1=32, offset2=40
    klog_info("PIC initialized: IRQs remapped to vectors 32-47\n");

    // Step 2: Disable all IRQs first
    pic_disable_all();

    // Step 3: Initialize the IDT
    idt_init();
    klog_info("IDT initialized\n");

    // Step 4: Register IRQ handlers
    idt_register_handler(IDT_IRQ0, (interrupt_handler_fn)timer_handler);
    klog_info("Timer interrupt handler registered\n");

    // Step 5: Enable only the timer IRQ (IRQ0)
    pic_enable_irq(0);

    // Step 6: Enable interrupts
    interrupt_enable();
    klog_info("Interrupts enabled\n");
}
