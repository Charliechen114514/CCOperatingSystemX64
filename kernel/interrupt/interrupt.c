/**
 * @file interrupt.c
 * @brief Interrupt subsystem implementation
 */

#include "interrupt.h"
#include "driver/pic/pic.h"
#include "idt.h"
#include "idt_constants.h"
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

    // Log first few timer interrupts to confirm they're happening
    if (timer_ticks <= 5) {
        klog_info("[TIMER] Tick #%lu received\n", timer_ticks);
        if (timer_ticks == 5) {
            klog_info("Welp, print it to here as INTR handles finished");
        }
    }

    // Send EOI to acknowledge the interrupt
    pic_send_eoi(0);
}

/**
 * @brief Get the current timer tick count
 * @return Number of timer interrupts since boot
 */
uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

/* ============================================================================
 * Interrupt Subsystem Initialization
 * ============================================================================ */

void interrupt_init(void) {
    klog_info("Initializing interrupt subsystem...\n");

    // Step 1: Initialize and remap the PIC
    // Remap IRQs 0-15 to vectors 32-47 to avoid CPU exceptions (0-31)
    pic_init(0x20, 0x28); // offset1=32, offset2=40
    klog_info("PIC initialized: IRQs remapped to vectors 32-47\n");

    // Step 2: Disable all IRQs first
    pic_disable_all();

    // Step 3: Initialize the IDT
    idt_init();
    klog_info("IDT initialized\n");

    // Step 4: Register IRQ handlers
    idt_register_handler(IDT_IRQ0, (interrupt_handler_fn)timer_handler);
    klog_info("Timer interrupt handler registered\n");

    // Step 5: Enable all IRQs
    for (int i = 0; i < 16; i++) {
        pic_enable_irq(i);
    }
    klog_info("All IRQs (0-15) enabled\n");

    // Step 6: Enable interrupts
    interrupt_enable();
    klog_info("Interrupts enabled\n");
}
