#include "welcome.h"
#include "klogs/kprintf.h"

extern void vga_display_welcome(void);
extern void serial_display_welcome(void);

void bootAllWelcomes(void) {
    /* Serial Welcomes */
    serial_display_welcome();
    klog_trace("Serial Display for the welcomes OK!\n");
    /* VGA Welcomes */
    vga_display_welcome();
    klog_trace("VGA Display for the welcomes OK!\n");
}