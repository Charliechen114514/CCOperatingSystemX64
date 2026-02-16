#include "serial.h"
#include "io/io.h"

/**
 * @brief Calculate divisor for desired baud rate.
 *        UART clock is 1.8432 MHz, divisor = clock / (16 * baud)
 *        For 115200 baud: divisor = 1843200 / (16 * 115200) = 1
 */
#define BAUD_DIVISOR_LOW 1  // Low byte of divisor
#define BAUD_DIVISOR_HIGH 0 // High byte of divisor

static void sync_serial_putc(char c) {
    const uint16_t base = SERIAL_COM1_BASE;

    // Wait until transmit hold register is empty
    while ((inb(SERIAL_LINE_STATUS_REG(base)) & SERIAL_LSR_READY_TO_SEND) == 0) {
        // Busy wait - CPU spins until ready
    }

    // Send the character
    outb(SERIAL_DATA_REG(base), (uint8_t)c);
}

bool serial_init(void) {
    const uint16_t base = SERIAL_COM1_BASE;

    // Step 1: Disable interrupts
    outb(SERIAL_INT_ENABLE_REG(base), 0x00);

    // Step 2: Enable DLAB (Divisor Latch Access Bit) to set baud rate
    outb(SERIAL_LINE_CTRL_REG(base), 0x80);

    // Step 3: Set divisor (low byte, then high byte)
    outb(SERIAL_DATA_REG(base), BAUD_DIVISOR_LOW);
    outb(SERIAL_INT_ENABLE_REG(base), BAUD_DIVISOR_HIGH);

    // Step 4: Configure line: 8 bits, no parity, 1 stop bit (8N1)
    // Bit 7: DLAB = 0
    // Bits 6-2: 0 (no parity, 1 stop bit)
    // Bits 1-0: 11 (8 data bits)
    outb(SERIAL_LINE_CTRL_REG(base), 0x03);

    // Step 5: Enable FIFO, clear buffers, set 14-byte threshold
    outb(SERIAL_FIFO_CTRL_REG(base), 0xC7);

    // Step 6: Enable IRQs, set RTS/DSR
    outb(SERIAL_MODEM_CTRL_REG(base), 0x0B);

    // Serial port initialized successfully
    // (Note: Skip hardware loopback test as it would send 0xAE to output)
    return true;
}

void sync_serial_puts(const char* str) {
    if (str == NULL) {
        return;
    }

    while (*str != '\0') {
        sync_serial_putc(*str);
        str++;
    }
}
