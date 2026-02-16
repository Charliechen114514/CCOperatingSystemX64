#include "welcome.h"

extern void vga_display_welcome(void);
extern void serial_display_welcome(void);

void bootAllWelcomes(void) {
    /* Serial Welcomes */
    serial_display_welcome();
    /* VGA Welcomes */
    vga_display_welcome();
}