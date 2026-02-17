/**
 * @file serial_intr.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Interrupt-driven serial communication implementation
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "serial_intr.h"
#include "driver/pic/pic.h"
#include "driver/serial/serial.h"
#include "interrupt/idt.h"
#include "io/io.h"
#include "klogs/ksnprintf.h"
#include "serial_config.h"

/* ============================================================================
 * Ring Buffer Implementation
 * ============================================================================ */

typedef struct {
    char buffer[UART_TX_BUFFER_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} ring_buffer_tx_t;

typedef struct {
    char buffer[UART_RX_BUFFER_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} ring_buffer_rx_t;

static inline bool ring_tx_is_empty(ring_buffer_tx_t* rb) {
    return rb->head == rb->tail;
}

static inline bool ring_tx_is_full(ring_buffer_tx_t* rb) {
    return ((rb->head + 1) % UART_TX_BUFFER_SIZE) == rb->tail;
}

static inline void ring_tx_write(ring_buffer_tx_t* rb, char c) {
    rb->buffer[rb->head] = c;
    rb->head = (rb->head + 1) % UART_TX_BUFFER_SIZE;
}

static inline char ring_tx_read(ring_buffer_tx_t* rb) {
    char c = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % UART_TX_BUFFER_SIZE;
    return c;
}

static inline bool ring_rx_is_empty(ring_buffer_rx_t* rb) {
    return rb->head == rb->tail;
}

static inline bool ring_rx_is_full(ring_buffer_rx_t* rb) {
    return ((rb->head + 1) % UART_RX_BUFFER_SIZE) == rb->tail;
}

static inline void ring_rx_write(ring_buffer_rx_t* rb, char c) {
    rb->buffer[rb->head] = c;
    rb->head = (rb->head + 1) % UART_RX_BUFFER_SIZE;
}

static inline char ring_rx_read(ring_buffer_rx_t* rb) {
    char c = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % UART_RX_BUFFER_SIZE;
    return c;
}

/* ============================================================================
 * UART State
 * ============================================================================ */

static ring_buffer_tx_t tx_buffer = {0};
static ring_buffer_rx_t rx_buffer = {0};

static volatile bool uart_echo_enabled = true;
static volatile bool uart_intr_mode_initialized = false;

// Forward declaration
static irq_descriptor_t uart_irq_desc;

/* ============================================================================
 * Interrupt Handler
 * ============================================================================ */

static void uart_irq_handler(interrupt_frame_t* frame, void* context) {
    (void)frame;
    (void)context;

    const uint16_t base = SERIAL_COM1_BASE;

    // Read interrupt identification register
    uint8_t iir = inb(SERIAL_INT_ID_REG(base));

    // Check if an interrupt is pending (bit 0 = 1 means no interrupt)
    if (iir & SERIAL_IIR_PENDING) {
        // Spurious interrupt
        pic_send_eoi(UART_COM1_IRQ);
        return;
    }

    // Get interrupt source (bits 1-3)
    uint8_t int_source = iir & SERIAL_IIR_SOURCE_MASK;

    // Debug: increment invocation count for diagnostics
    uart_irq_desc.invocation_count++;

    switch (int_source) {
        case SERIAL_IIR_RX_READY: // Receiver data available (or timeout with FIFO)
        {
            // Read all available characters
            while (inb(SERIAL_LINE_STATUS_REG(base)) & SERIAL_LSR_DATA_READY) {
                char c = (char)inb(SERIAL_DATA_REG(base));

                // Store in receive buffer if not full
                if (!ring_rx_is_full(&rx_buffer)) {
                    ring_rx_write(&rx_buffer, c);
                }
            }
            break;
        }

        case SERIAL_IIR_TX_EMPTY: // Transmitter holding register empty
        {
            if (!ring_tx_is_empty(&tx_buffer)) {
                char c = ring_tx_read(&tx_buffer);
                outb(SERIAL_DATA_REG(base), (uint8_t)c);
            } else {
                // Buffer empty, disable TX interrupt
                uint8_t ier = inb(SERIAL_INT_ENABLE_REG(base));
                outb(SERIAL_INT_ENABLE_REG(base), ier & ~SERIAL_IER_TX_EMPTY);
            }
            break;
        }

        case SERIAL_IIR_RX_STATUS: // Receiver line status
        {
            // Read line status register to clear the interrupt
            inb(SERIAL_LINE_STATUS_REG(base));
            break;
        }

        case 0x0C: // Character timeout indication (with FIFO)
        {
            // Read all available characters
            while (inb(SERIAL_LINE_STATUS_REG(base)) & SERIAL_LSR_DATA_READY) {
                char c = (char)inb(SERIAL_DATA_REG(base));

                if (!ring_rx_is_full(&rx_buffer)) {
                    ring_rx_write(&rx_buffer, c);
                }
            }
            break;
        }

        default:
            // Other interrupt sources - just clear
            break;
    }

    // Send EOI to PIC
    pic_send_eoi(UART_COM1_IRQ);
}

static irq_descriptor_t uart_irq_desc = {.name = "UART COM1",
                                         .handler = uart_irq_handler,
                                         .context = NULL,
                                         .flags = IRQ_FLAG_NONE,
                                         .invocation_count = 0};

/* ============================================================================
 * Public Functions
 * ============================================================================ */

int uart_init_intr_mode(void) {
    const uint16_t base = SERIAL_COM1_BASE;

    if (uart_intr_mode_initialized) {
        return 0;
    }

    // Read and clear any pending interrupts and data
    inb(SERIAL_INT_ID_REG(base));
    inb(SERIAL_LINE_STATUS_REG(base));
    inb(SERIAL_DATA_REG(base));

    // Ensure FIFO is enabled and configured
    // 0xC7 = Enable FIFO, clear both FIFOs, 14-byte interrupt threshold
    outb(SERIAL_FIFO_CTRL_REG(base), 0xC7);

    // Register the IRQ handler
    int result = irq_register_handler(UART_COM1_IRQ, &uart_irq_desc);
    if (result != 0) {
        return result;
    }

    // Enable receiver data available interrupt
    // Also enable receiver line status interrupt for error detection
    outb(SERIAL_INT_ENABLE_REG(base), SERIAL_IER_RX_READY | SERIAL_IER_RX_STATUS);

    // Enable the IRQ line in the PIC
    pic_enable_irq(UART_COM1_IRQ);

    uart_intr_mode_initialized = true;
    return 0;
}

void async_serial_putc(char c) {
    const uint16_t base = SERIAL_COM1_BASE;

    if (!uart_intr_mode_initialized) {
        // Fall back to synchronous mode
        while ((inb(SERIAL_LINE_STATUS_REG(base)) & SERIAL_LSR_THRE) == 0) {
            __asm__ volatile("pause");
        }
        outb(SERIAL_DATA_REG(base), (uint8_t)c);
        return;
    }

    // Wait for space in the buffer
    while (ring_tx_is_full(&tx_buffer)) {
        __asm__ volatile("pause");

        // Kick transmission if stalled
        uint8_t ier = inb(SERIAL_INT_ENABLE_REG(base));
        if ((ier & SERIAL_IER_TX_EMPTY) == 0 && !ring_tx_is_empty(&tx_buffer)) {
            outb(SERIAL_INT_ENABLE_REG(base), ier | SERIAL_IER_TX_EMPTY);
        }
    }

    // Disable interrupts briefly to protect the buffer
    uint64_t rflags;
    __asm__ volatile("pushfq; popq %0" : "=r"(rflags));
    __asm__ volatile("cli");

    bool was_empty = ring_tx_is_empty(&tx_buffer);
    ring_tx_write(&tx_buffer, c);

    // Restore interrupts
    if (rflags & 0x200) {
        __asm__ volatile("sti");
    }

    // Start transmission if buffer was empty
    if (was_empty) {
        // Check if THR is already empty (it usually is)
        if (inb(SERIAL_LINE_STATUS_REG(base)) & SERIAL_LSR_THRE) {
            // Send the first character immediately
            char c = ring_tx_read(&tx_buffer);
            outb(SERIAL_DATA_REG(base), c);
            // Enable TX interrupt for remaining characters
            if (!ring_tx_is_empty(&tx_buffer)) {
                outb(SERIAL_INT_ENABLE_REG(base),
                     inb(SERIAL_INT_ENABLE_REG(base)) | SERIAL_IER_TX_EMPTY);
            }
        } else {
            // THR is not empty, enable interrupt to send when ready
            outb(SERIAL_INT_ENABLE_REG(base),
                 inb(SERIAL_INT_ENABLE_REG(base)) | SERIAL_IER_TX_EMPTY);
        }
    }
}

void async_serial_puts(const char* str) {
    if (str == NULL) {
        return;
    }

    while (*str != '\0') {
        async_serial_putc(*str);
        str++;
    }
}

bool uart_haschar(void) {
    return !ring_rx_is_empty(&rx_buffer);
}

char uart_getchar(void) {
    while (ring_rx_is_empty(&rx_buffer)) {
        __asm__ volatile("pause");
    }

    uint64_t rflags;
    __asm__ volatile("pushfq; popq %0" : "=r"(rflags));
    __asm__ volatile("cli");

    char c = ring_rx_read(&rx_buffer);

    if (rflags & 0x200) {
        __asm__ volatile("sti");
    }

    return c;
}

int uart_try_getchar(char* c) {
    if (c == NULL) {
        return -1;
    }

    if (ring_rx_is_empty(&rx_buffer)) {
        return -1;
    }

    uint64_t rflags;
    __asm__ volatile("pushfq; popq %0" : "=r"(rflags));
    __asm__ volatile("cli");

    *c = ring_rx_read(&rx_buffer);

    if (rflags & 0x200) {
        __asm__ volatile("sti");
    }

    return 0;
}

void uart_set_echo(bool enable) {
    uart_echo_enabled = enable;
}

bool uart_get_echo(void) {
    return uart_echo_enabled;
}

uint64_t uart_get_interrupt_count(void) {
    return uart_irq_desc.invocation_count;
}

void uart_dump_registers(void) {
    const uint16_t base = SERIAL_COM1_BASE;
    char buf[128];

    uint8_t ier = inb(SERIAL_INT_ENABLE_REG(base));
    uint8_t iir = inb(SERIAL_INT_ID_REG(base));
    uint8_t lcr = inb(SERIAL_LINE_CTRL_REG(base));
    uint8_t lsr = inb(SERIAL_LINE_STATUS_REG(base));
    uint8_t mcr = inb(SERIAL_MODEM_CTRL_REG(base));
    uint8_t msr = inb(base + 6); // Modem status register

    // Use ksnprintf to format the output
    ksnprintf(buf, sizeof(buf), "IER=%02X IIR=%02X LCR=%02X LSR=%02X MCR=%02X MSR=%02X\n", ier, iir,
              lcr, lsr, mcr, msr);
    sync_serial_puts(buf);

    ksnprintf(buf, sizeof(buf), "IRQ count: %lu\n", uart_irq_desc.invocation_count);
    sync_serial_puts(buf);
}
