/**
 * @file pic.c
 * @brief 8259 PIC implementation
 */

#include "pic.h"
#include "io/io.h"
#include "pic_constants.h"

/* ============================================================================
 * Internal Macros
 * ============================================================================ */

#ifndef BIT
#    define BIT(n) (1UL << (n))
#endif

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Send a command to the PIC
 */
static void pic_send_command(uint8_t cmd, uint16_t port) {
    outb(port, cmd);
}

/**
 * @brief Send data to the PIC
 */
static void pic_send_data(uint8_t data, uint16_t port) {
    outb(port, data);
}

/**
 * @brief Read data from the PIC
 */
static uint8_t pic_read_data(uint16_t port) {
    return inb(port);
}

/* ============================================================================
 * Public Functions
 * ============================================================================ */

void pic_init(uint8_t offset1, uint8_t offset2) {
    // Save original masks
    uint8_t a1 = pic_read_data(PIC1_DATA);
    uint8_t a2 = pic_read_data(PIC2_DATA);

    // Start initialization sequence (ICW1)
    pic_send_command(PIC_INIT, PIC1_CMD);
    pic_send_command(PIC_INIT, PIC2_CMD);

    // Set vector offsets (ICW2)
    pic_send_data(offset1, PIC1_DATA);
    pic_send_data(offset2, PIC2_DATA);

    // Configure cascading (ICW3)
    // Master PIC: IR line 2 is connected to slave
    pic_send_data(0x04, PIC1_DATA);
    // Slave PIC: cascade identity
    pic_send_data(0x02, PIC2_DATA);

    // Set 8086 mode (ICW4)
    pic_send_data(PIC_ICW4_8086, PIC1_DATA);
    pic_send_data(PIC_ICW4_8086, PIC2_DATA);

    // Restore original masks (all IRQs disabled by default)
    pic_send_data(a1, PIC1_DATA);
    pic_send_data(a2, PIC2_DATA);
}

void pic_send_eoi(uint8_t irq) {
    // If IRQ is from slave PIC (IRQ >= 8), we need to send EOI to slave too
    if (irq >= 8) {
        pic_send_command(PIC_EOI, PIC2_CMD);
    }
    // Always send EOI to master PIC
    pic_send_command(PIC_EOI, PIC1_CMD);
}

void pic_disable_irq(uint8_t irq) {
    uint16_t port;
    uint8_t mask;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    mask = pic_read_data(port) | BIT(irq);
    pic_send_data(mask, port);
}

void pic_enable_irq(uint8_t irq) {
    uint16_t port;
    uint8_t mask;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    mask = pic_read_data(port) & ~BIT(irq);
    pic_send_data(mask, port);
}

bool pic_is_irq_masked(uint8_t irq) {
    uint16_t port;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    return (pic_read_data(port) & BIT(irq)) != 0;
}

void pic_disable_all(void) {
    pic_send_data(0xFF, PIC1_DATA);
    pic_send_data(0xFF, PIC2_DATA);
}

void pic_enable_all(void) {
    pic_send_data(0x00, PIC1_DATA);
    pic_send_data(0x00, PIC2_DATA);
}
