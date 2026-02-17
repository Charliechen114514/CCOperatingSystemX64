/**
 * @file timer.c
 * @brief PIT 8253/8254 Timer Driver Implementation
 */

#include "timer.h"
#include "driver/io/io.h"
#include "driver/pic/pic.h"
#include "interrupt/idt.h"
#include "interrupt/idt_constants.h"
#include "klogs/kprintf.h"
#include "timer_constants.h"

/* ============================================================================
 * Timer State
 * ============================================================================ */

static volatile uint64_t timer_ticks = 0;
static uint32_t timer_frequency = 0;
static timer_callback_fn timer_callback = NULL;

/* ============================================================================
 * IRQ Descriptor for Timer
 * ============================================================================ */

static irq_descriptor_t timer_irq_desc = {.name = "PIT Timer",
                                          .handler = timer_irq_handler,
                                          .context = NULL,
                                          .flags = IRQ_FLAG_NONE,
                                          .invocation_count = 0};

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

/**
 * @brief Configure PIT frequency
 *
 * The PIT input clock is approximately 1.19318 MHz.
 * divisor = 1193180 / frequency
 *
 * @param frequency Desired frequency in Hz
 */
static void pit_set_frequency(uint32_t frequency) {
    uint32_t divisor = PIT_BASE_FREQUENCY / frequency;

    // Send control word: binary mode, mode 3 (square wave), lobyte/hibyte access
    outb(PIT_CMD_PORT, PIT_CHANNEL_0 | PIT_MODE_3 | PIT_ACCESS_WORD);

    // Send divisor (low byte then high byte)
    outb(PIT_CHANNEL_0_DATA, divisor & 0xFF);
    outb(PIT_CHANNEL_0_DATA, (divisor >> 8) & 0xFF);
}

/* ============================================================================
 * Timer Interrupt Handler
 * ============================================================================ */

void timer_irq_handler(interrupt_frame_t* frame, void* context) {
    (void)frame;
    (void)context;

    timer_ticks++;
    // Call registered callback if present
    if (timer_callback != NULL) {
        timer_callback(timer_ticks);
    }
}

/* ============================================================================
 * Public Functions
 * ============================================================================ */

int timer_init(uint32_t frequency) {
    if (frequency == 0) {
        frequency = TIMER_DEFAULT_FREQUENCY;
    }

    klog_trace("Initializing PIT timer at %u Hz\n", frequency);
    // Save frequency
    timer_frequency = frequency;
    // Configure PIT
    pit_set_frequency(frequency);
    // Register IRQ handler using the new registration mechanism
    int result = irq_register_handler(0, &timer_irq_desc);
    if (result != 0) {
        klog_error("Failed to register timer IRQ handler: %d\n", result);
        return result;
    }

    klog_trace("Timer initialized and IRQ handler registered\n");
    return 0;
}

uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

void timer_mdelay(uint32_t milliseconds) {
    uint64_t start = timer_ticks;
    uint64_t target_ticks = start + (milliseconds * timer_frequency) / 1000;

    while (timer_ticks < target_ticks) {
        __asm__ volatile("pause");
    }
}

void timer_set_callback(timer_callback_fn callback) {
    timer_callback = callback;
}

int timer_set_frequency(uint32_t frequency) {
    // PIT frequency constraints: divisor must be >= 1
    // divisor = 1193180 / frequency, so frequency <= 1193180
    // Also, divisor <= 65536, so frequency >= 1193180 / 65536 ≈ 18
    if (frequency < 18 || frequency > PIT_BASE_FREQUENCY) {
        klog_error("[TIMER] Invalid frequency %u Hz (valid range: 18-1193180 Hz)\n", frequency);
        return -1;
    }

    klog_trace("[TIMER] Changing frequency from %u Hz to %u Hz\n", timer_frequency, frequency);

    // Update frequency
    timer_frequency = frequency;

    // Reconfigure PIT
    pit_set_frequency(frequency);

    return 0;
}
