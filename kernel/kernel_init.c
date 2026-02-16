#include "kernel_init.h"
#include "driver/serial/serial.h"
#include "driver/vga/vga.h"
#include "welcomes/welcome.h"

void kernel_init(void) {
    // Initialize serial port first for early debug output
    serial_init();
    system_vga_init();

    /* One must Ensure the backends have been bootified, else sucks! */
    bootAllWelcomes();
}
