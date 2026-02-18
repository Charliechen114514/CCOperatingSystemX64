#include "kernel_init.h"
#include "base/hashmap.h"
#include "driver/keyboard/keyboard.h"
#include "driver/rtc/rtc.h"
#include "driver/serial/serial.h"
#include "driver/serial/serial_intr.h"
#include "driver/timer/timer.h"
#include "driver/vga/vga.h"
#include "interrupt/exception.h"
#include "interrupt/gdt.h"
#include "interrupt/interrupt.h"
#include "interrupt/tss.h"
#include "klogs/kprintf.h"
#include "klogs/kprintf_config.h"
#include "mm/heap/heap.h"
#include "mm/memory_detect/e820.h"
#include "mm/pframe/pframe.h"
#include "mm/vmm/cow.h"
#include "mm/vmm/fault.h"
#include "mm/vmm/page.h"
#include "mm/vmm/vmm.h"
#include "shell/backends/serial_shell.h"
#include "shell/backends/vga_shell.h"
#include "syscall/syscall.h"
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

    // Print available physical memory
    mem_stats_t mem_stats;
    e820_get_stats(&mem_stats);
    klog_trace("[MEM] Total: %u MB, Usable: %u MB, Entries: %u\n", mem_stats.total_mb,
               mem_stats.usable_mb, mem_stats.entry_count);

    // Initialize physical frame allocator
    pframe_init();

    // Initialize page table management
    page_init();

    // Initialize virtual memory manager
    vmm_init();

    // Initialize kernel heap (kmalloc/kfree)
    heap_init();

    // Phase 1: Initialize TSS and GDT (must be before IDT)
    tss_init(); /* Initialize TSS with IST stacks */
    gdt_init(); /* Initialize GDT and load TSS */

    // Phase 2: Initialize interrupt subsystem (PIC + IDT, but interrupts disabled)
    // NOTE: syscall_init is moved after interrupt_init to ensure IDT is ready
    // in case any MSR/CR4 operations trigger exceptions.
    interrupt_init();

    // Phase 3: Register exception handlers
    exception_init(); /* Register DF, SS, GP handlers */

    // Phase 3.5: Initialize system call framework (after IDT and exception handlers)
    syscall_init(); /* Initialize syscall/sysret framework */

    // Phase 4: Initialize COW subsystem (depends on hashmap and heap)
    cow_init(); /* Initialize copy-on-write tracking */

    // Phase 5: Initialize page fault handler (depends on COW)
    pf_init();

    // Phase 6: Initialize all interrupt-dependent devices
    // They will register their IRQ handlers during this phase
    timer_init(0);         // 0 = use default frequency (1000 Hz)
    rtc_init();            // Initialize RTC (periodic interrupt disabled by default)
    uart_init_intr_mode(); // Initialize UART interrupt mode for interactive communication
    keyboard_init();       // Initialize keyboard driver for VGA shell

    // Phase 7: Finalize interrupt initialization (enable IRQs + CPU interrupts)
    interrupt_finalize();

    // Initialize shell-specific commands
    // serial_shell_init_commands();  // Serial shell commands (time, ticks, echo, uart)
    // vga_shell_init_commands();     // VGA shell commands (cls, color, goto, keyboard)
    /* One must Ensure the backends have been bootified, else sucks! */
    // bootAllWelcomes();
    klog_trace("Boot Welcomes Done!\n");
    klog_info("kernel init finished!\n");
}
