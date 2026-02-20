#include "kernel_init.h"
#include "driver/keyboard/keyboard.h"
#include "driver/rtc/rtc.h"
#include "driver/serial/serial.h"
#include "driver/serial/serial_intr.h"
#include "driver/timer/timer.h"
#include "driver/vga/vga.h"
#include "fs/fs.h"
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
#include "process/process.h"
#include "syscall/syscall.h"

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
    // Initialize page table management FIRST
    // This establishes the direct physical mapping needed to access .lbss (2MB bitmap)
    page_init();

    // Now initialize physical frame allocator
    // The .lbss section has been cleared by page_init(), so bitmap_init can safely use it
    pframe_init();

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
    klog_trace("[INIT] Before syscall_init...\n");
    syscall_init(); /* Initialize syscall/sysret framework */
    klog_trace("[INIT] After syscall_init, before cow_init...\n");

    // Phase 4: Initialize COW subsystem (depends on hashmap and heap)
    cow_init(); /* Initialize copy-on-write tracking */
    klog_trace("[INIT] After cow_init, before pf_init...\n");

    // Phase 5: Initialize page fault handler (depends on COW)
    pf_init();
    klog_trace("[INIT] After pf_init...\n");

    // Phase 6: Initialize all interrupt-dependent devices
    // They will register their IRQ handlers during this phase
    timer_init(0);         // 0 = use default frequency (1000 Hz)
    rtc_init();            // Initialize RTC (periodic interrupt disabled by default)
    uart_init_intr_mode(); // Initialize UART interrupt mode for interactive communication
    keyboard_init();       // Initialize keyboard driver for VGA shell

    // Initialize process subsystem (must be after timer for scheduler ticks)
    proc_init();

    // Initialize filesystem subsystem (must be after heap)
    fs_init();
    // Phase 7: Finalize interrupt initialization (enable IRQs + CPU interrupts)
    interrupt_finalize();

    // Initialize shell-specific commands
    // serial_shell_init_commands();  // Serial shell commands (time, ticks, echo, uart)
    // vga_shell_init_commands();     // VGA shell commands (cls, color, goto, keyboard)
    /* One must Ensure the backends have been bootified, else sucks! */

    klog_info("kernel init finished!\n");

    /* NOTE: bootAllWelcomes() and run_possible_demos() are now called from init_thread
     * which is created in proc_init() and runs with high priority after scheduling starts.
     * This ensures they run in a proper scheduling context.
     *
     * IMPORTANT: We need to trigger the first schedule to start the scheduler.
     * The init_thread (PID=1) is already enqueued with high priority.
     */
    klog_info("[INIT] Triggering first schedule to start init_thread...\n");

    /* Trigger the first schedule - this will switch to init_thread
     * Note: interrupts are already enabled by interrupt_finalize() above */
    extern void schedule(void);
    schedule();

    /* Should never reach here - init_thread will take over */
    klog_error("[INIT] schedule() returned unexpectedly!\n");
}
