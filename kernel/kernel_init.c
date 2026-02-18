#include "kernel_init.h"
#include "driver/keyboard/keyboard.h"
#include "driver/rtc/rtc.h"
#include "driver/serial/serial.h"
#include "driver/serial/serial_intr.h"
#include "driver/timer/timer.h"
#include "driver/vga/vga.h"
#include "interrupt/interrupt.h"
#include "klogs/kprintf.h"
#include "klogs/kprintf_config.h"
#include "mm/memory_detect/e820.h"
#include "mm/pframe/pframe.h"
#include "shell/backends/serial_shell.h"
#include "shell/backends/vga_shell.h"
#include "welcomes/welcome.h"

// Forward declaration for demo controller (always available, checks internally)
void run_possible_demos(void);

static void driver_subsystem_inits(void) {
    // Initialize serial port first for early debug output
    serial_init();
    sync_serial_puts("Boot the serails OK\n");
    system_vga_init();
    sync_serial_puts("Boot the VGA OK\n");

    sync_serial_puts("=== === === === === ===!\n");
    sync_serial_puts("Boot All device OK!\n");
    sync_serial_puts("=== === === === === ===!\n");
}

void kernel_init(void) {
    driver_subsystem_inits();

    klog_init(KPRINTF_DEFAULT_BACKEND);
    klog_trace("klog self boot OK, one can log the kernel logs!\n");

    // Initialize memory detection (parse E820 map from bootloader)
    e820_init();

    // Print full E820 memory map for debugging
    e820_dump_map();

    // Print available physical memory
    mem_stats_t mem_stats;
    e820_get_stats(&mem_stats);
    klog_trace("[MEM] Total: %u MB, Usable: %u MB, Entries: %u\n", mem_stats.total_mb,
               mem_stats.usable_mb, mem_stats.entry_count);

    // Initialize physical frame allocator
    pframe_init();
    pframe_dump();

    /* One must Ensure the backends have been bootified, else sucks! */
    bootAllWelcomes();
    klog_trace("Boot Welcomes Done!\n");

    // Phase 1: Initialize interrupt subsystem (PIC + IDT, but interrupts disabled)
    interrupt_init();

    // Phase 2: Initialize all interrupt-dependent devices
    // They will register their IRQ handlers during this phase
    timer_init(0);         // 0 = use default frequency (1000 Hz)
    rtc_init();            // Initialize RTC (periodic interrupt disabled by default)
    uart_init_intr_mode(); // Initialize UART interrupt mode for interactive communication
    keyboard_init();       // Initialize keyboard driver for VGA shell

    // Phase 3: Finalize interrupt initialization (enable IRQs + CPU interrupts)
    interrupt_finalize();

    // Initialize shell-specific commands
    // serial_shell_init_commands();  // Serial shell commands (time, ticks, echo, uart)
    // vga_shell_init_commands();     // VGA shell commands (cls, color, goto, keyboard)

    klog_info("kernel init finished!\n");
}
