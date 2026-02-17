#include "kernel_init.h"
#include "driver/serial/serial.h"
#include "driver/vga/vga.h"
#include "klogs/kprintf.h"
#include "klogs/kprintf_config.h"
#include "welcomes/welcome.h"
#include "interrupt/interrupt.h"

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
    klog_debug("klog self boot OK, one can log the kernel logs!\n");
    /* One must Ensure the backends have been bootified, else sucks! */
    bootAllWelcomes();
    klog_trace("Boot Welcomes Done!\n");

    // Initialize interrupt subsystem (must be after klog_init for logging)
    interrupt_init();

    klog_info("kernel init finished!\n");
}
