#include "welcome.h"
#include "klogs/kprintf.h"

extern void vga_display_welcome(void);
extern void serial_display_welcome(void);

void bootAllWelcomes(void) {
    /* NOTE: This function now runs in init_thread (PID=1) with high priority,
     * so we don't need to disable preemption. The init_thread will run to
     * completion before the scheduler switches to other tasks. */

    /* Serial Welcomes */
    serial_display_welcome();
    klog_trace("Serial Display for the welcomes OK!\n");
    /* VGA Welcomes */
    vga_display_welcome();
    klog_trace("VGA Display for the welcomes OK!\n");
}