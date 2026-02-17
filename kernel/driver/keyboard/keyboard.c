/**
 * @file keyboard.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief PS/2 Keyboard driver implementation
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "keyboard.h"
#include "keyboard_config.h"
#include "driver/pic/pic.h"
#include "interrupt/idt.h"
#include "io/io.h"
#include "klogs/ksnprintf.h"

/* ============================================================================
 * Ring Buffer Implementation
 * ============================================================================ */

typedef struct {
    char buffer[KEYBOARD_BUFFER_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} ring_buffer_kb_t;

static inline bool ring_kb_is_empty(ring_buffer_kb_t* rb) {
    return rb->head == rb->tail;
}

static inline bool ring_kb_is_full(ring_buffer_kb_t* rb) {
    return ((rb->head + 1) % KEYBOARD_BUFFER_SIZE) == rb->tail;
}

static inline void ring_kb_write(ring_buffer_kb_t* rb, char c) {
    rb->buffer[rb->head] = c;
    rb->head = (rb->head + 1) % KEYBOARD_BUFFER_SIZE;
}

static inline char ring_kb_read(ring_buffer_kb_t* rb) {
    char c = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

/* ============================================================================
 * Scancode to ASCII Mapping (US QWERTY, Set 1)
 * ============================================================================ */

// Standard scancode to ASCII mapping (no shift)
static const char scancode_to_ascii_table[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',       // 0x00-0x0F
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,   // 0x10-0x1F
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z',   // 0x20-0x2F
    'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0,     // 0x30-0x3F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                               // 0x40-0x4F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                               // 0x50-0x5F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                               // 0x60-0x6F
    0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0                            // 0x70-0x7F
};

// Shifted characters mapping
static const char scancode_to_ascii_shifted[128] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',        // 0x00-0x0F
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,    // 0x10-0x1F
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z',       // 0x20-0x2F
    'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0,      // 0x30-0x3F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                // 0x40-0x4F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                // 0x50-0x5F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                // 0x60-0x6F
    0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0                             // 0x70-0x7F
};

/* ============================================================================
 * Keyboard State
 * ============================================================================ */

static ring_buffer_kb_t kb_buffer = {0};
static volatile bool kb_shift_pressed = false;
static volatile bool kb_caps_lock = false;
static volatile bool kb_initialized = false;

static irq_descriptor_t keyboard_irq_desc = {
    .name = "PS/2 Keyboard",
    .handler = NULL,
    .context = NULL,
    .flags = IRQ_FLAG_NONE,
    .invocation_count = 0
};

/* ============================================================================
 * Scancode Conversion
 * ============================================================================ */

/**
 * @brief Check if a scancode is for a letter key
 */
static inline bool is_letter_scancode(uint8_t scancode) {
    // Letters are: QWERTY row (0x10-0x1C) and ASDF row (0x1E-0x26)
    // Note: This is a simplified check
    return (scancode >= 0x10 && scancode <= 0x1C) ||
           (scancode >= 0x1E && scancode <= 0x26);
}

/**
 * @brief Convert scancode to ASCII character
 */
static char scancode_to_ascii(uint8_t scancode) {
    // Check if it's a break code (key release)
    if (scancode & SCANCODE_BREAK_MASK) {
        return 0;  // Don't convert break codes to ASCII
    }

    if (scancode >= 128) {
        return 0;
    }

    // Apply shift and caps lock logic
    bool use_shift = kb_shift_pressed;

    // Caps Lock only affects letters
    if (kb_caps_lock && is_letter_scancode(scancode)) {
        use_shift = !use_shift;
    }

    if (use_shift) {
        return scancode_to_ascii_shifted[scancode];
    } else {
        return scancode_to_ascii_table[scancode];
    }
}

/* ============================================================================
 * Interrupt Handler
 * ============================================================================ */

static void keyboard_irq_handler(interrupt_frame_t* frame, void* context) {
    (void)frame;
    (void)context;

    // Read status
    uint8_t status = inb(KEYBOARD_STATUS_PORT);

    // Check if output buffer is full (data available)
    if (!(status & KBD_STATUS_OUTPUT_FULL)) {
        // Spurious interrupt or no data
        pic_send_eoi(KEYBOARD_IRQ);
        return;
    }

    // Read scancode
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Handle modifier keys and regular keys
    uint8_t code = scancode & 0x7F;  // Remove break bit
    bool is_break = (scancode & SCANCODE_BREAK_MASK) != 0;

    switch (code) {
        case SCANCODE_LSHIFT:
        case SCANCODE_RSHIFT:
            kb_shift_pressed = !is_break;
            break;

        case SCANCODE_CAPSLOCK:
            if (!is_break) {
                // Toggle Caps Lock on make only
                kb_caps_lock = !kb_caps_lock;
            }
            break;

        default: {
            // Convert scancode to ASCII and store if it's a make code
            if (!is_break) {
                char c = scancode_to_ascii(scancode);
                if (c != 0 && !ring_kb_is_full(&kb_buffer)) {
                    ring_kb_write(&kb_buffer, c);
                }
            }
            break;
        }
    }

    keyboard_irq_desc.invocation_count++;
    pic_send_eoi(KEYBOARD_IRQ);
}

/* ============================================================================
 * Public Functions
 * ============================================================================ */

int keyboard_init(void) {
    if (kb_initialized) {
        return 0;
    }

    // Set the handler in the descriptor
    keyboard_irq_desc.handler = keyboard_irq_handler;

    // Flush any pending data from the keyboard
    while (inb(KEYBOARD_STATUS_PORT) & KBD_STATUS_OUTPUT_FULL) {
        inb(KEYBOARD_DATA_PORT);
    }

    // Register the IRQ handler
    int result = irq_register_handler(KEYBOARD_IRQ, &keyboard_irq_desc);
    if (result != 0) {
        return result;
    }

    // Enable keyboard scanning
    outb(KEYBOARD_DATA_PORT, KBD_CMD_ENABLE);

    // Enable the IRQ line in the PIC
    pic_enable_irq(KEYBOARD_IRQ);

    kb_initialized = true;
    return 0;
}

bool keyboard_haschar(void) {
    return !ring_kb_is_empty(&kb_buffer);
}

char keyboard_getchar(void) {
    while (ring_kb_is_empty(&kb_buffer)) {
        __asm__ volatile("pause");
    }

    // Disable interrupts briefly to protect the buffer
    uint64_t rflags;
    __asm__ volatile("pushfq; popq %0" : "=r"(rflags));
    __asm__ volatile("cli");

    char c = ring_kb_read(&kb_buffer);

    // Restore interrupt state
    if (rflags & 0x200) {
        __asm__ volatile("sti");
    }

    return c;
}

int keyboard_try_getchar(char* c) {
    if (c == NULL) {
        return -1;
    }

    if (ring_kb_is_empty(&kb_buffer)) {
        return -1;
    }

    // Disable interrupts briefly to protect the buffer
    uint64_t rflags;
    __asm__ volatile("pushfq; popq %0" : "=r"(rflags));
    __asm__ volatile("cli");

    *c = ring_kb_read(&kb_buffer);

    // Restore interrupt state
    if (rflags & 0x200) {
        __asm__ volatile("sti");
    }

    return 0;
}

uint64_t keyboard_get_interrupt_count(void) {
    return keyboard_irq_desc.invocation_count;
}
