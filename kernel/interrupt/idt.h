/**
 * @file idt.h
 * @brief Interrupt Descriptor Table (IDT) definitions and management for x86_64
 *
 * The IDT is used by the x86_64 processor to determine the correct response
 * to interrupts and exceptions.
 */

#pragma once

#include "defines/types.h"

/* ============================================================================
 * Interrupt Stack Frame (pushed by CPU on interrupt)
 * ============================================================================ */

/**
 * @brief Stack frame pushed by x86_64 CPU on interrupt/exception
 *
 * This structure represents the exact layout of data pushed onto the stack
 * by the CPU when an interrupt or exception occurs.
 */
typedef struct PACKED {
    uint64_t error_code; // Error code (pushed only for some exceptions)
    uint64_t rip;        // Instruction pointer
    uint64_t cs;         // Code segment
    uint64_t rflags;     // RFLAGS register
    uint64_t rsp;        // Stack pointer
    uint64_t ss;         // Stack segment
} interrupt_frame_t;

/* ============================================================================
 * Exception Handler Function Type
 * ============================================================================ */

/**
 * @brief Type for exception/IRQ handler functions
 *
 * @param frame Pointer to the interrupt stack frame
 * @param error_code Error code (0 for most exceptions)
 */
typedef void (*interrupt_handler_fn)(interrupt_frame_t* frame, uint64_t error_code);

/* ============================================================================
 * IRQ Handler Registration Types (New)
 * ============================================================================ */

/**
 * @brief IRQ handler flags
 */
typedef enum irq_handler_flags {
    IRQ_FLAG_NONE = 0,
    IRQ_FLAG_AUTOEOI = (1 << 0),  // Handler sends EOI automatically
} irq_handler_flags_t;

/**
 * @brief New IRQ handler function type with context support
 *
 * @param frame Pointer to the interrupt stack frame
 * @param context Context pointer passed during registration
 */
typedef void (*irq_handler_fn)(interrupt_frame_t* frame, void* context);

/**
 * @brief IRQ descriptor - describes an IRQ handler
 */
typedef struct irq_descriptor {
    const char* name;              // Handler name (for debugging)
    irq_handler_fn handler;        // Handler function
    void* context;                 // Context pointer passed to handler
    irq_handler_flags_t flags;     // Handler flags
    uint64_t invocation_count;     // Statistics: number of times called
} irq_descriptor_t;

/* ============================================================================
 * IDT Management Functions
 * ============================================================================ */

/**
 * @brief Initialize the IDT
 *
 * Sets up all IDT entries with default handlers and loads the IDT.
 */
void idt_init(void);

/**
 * @brief Set an IDT entry
 *
 * @param vector Interrupt vector number (0-255)
 * @param handler Pointer to the interrupt handler function
 * @param type_attr Type attributes (e.g., IDT_KERNEL_INTERRUPT_GATE)
 * @param segment_selector Code segment selector (usually 0x08 for kernel code)
 */
void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t type_attr, uint16_t segment_selector);

/**
 * @brief Register a custom interrupt handler
 *
 * @param vector Interrupt vector number
 * @param handler Handler function
 */
void idt_register_handler(uint8_t vector, interrupt_handler_fn handler);

/* ============================================================================
 * New IRQ Registration API
 * ============================================================================ */

/**
 * @brief Register an IRQ handler with descriptor
 *
 * @param irq IRQ number (0-15)
 * @param descriptor Pointer to irq_descriptor_t (caller allocated, typically static)
 * @return int 0 on success, negative on error
 */
int irq_register_handler(uint8_t irq, irq_descriptor_t* descriptor);

/**
 * @brief Unregister an IRQ handler
 *
 * @param irq IRQ number (0-15)
 * @param descriptor Pointer to irq_descriptor_t to unregister
 * @return int 0 on success, negative on error
 */
int irq_unregister_handler(uint8_t irq, irq_descriptor_t* descriptor);

/**
 * @brief Simple IRQ registration macro
 *
 * Usage: IRQ_REGISTER_SIMPLE(0, timer_irq_handler, "PIT Timer")
 */
#define IRQ_REGISTER_SIMPLE(irq, handler_fn, name_str) \
    do { \
        static irq_descriptor_t __desc_##handler_fn = { \
            .name = (name_str), \
            .handler = (irq_handler_fn)(handler_fn), \
            .context = NULL, \
            .flags = IRQ_FLAG_NONE, \
            .invocation_count = 0 \
        }; \
        irq_register_handler((irq), &__desc_##handler_fn); \
    } while(0)

/**
 * @brief Get the name of an exception/vector
 *
 * @param vector Interrupt vector number
 * @return const char* Name of the exception or "Unknown"
 */
const char* idt_get_exception_name(uint8_t vector);

/* ============================================================================
 * Architecture-Specific Functions (implemented in interrupt.s)
 * ============================================================================ */

/**
 * @brief Load the IDT (lidt instruction)
 *
 * @param idt_ptr Pointer to the IDT pointer structure
 */
void idt_load(uint64_t idt_ptr);

/**
 * @brief Enable interrupts (sti instruction)
 */
static inline void interrupt_enable(void) {
    __asm__ volatile("sti");
}

/**
 * @brief Disable interrupts (cli instruction)
 */
static inline void interrupt_disable(void) {
    __asm__ volatile("cli");
}

/**
 * @brief Check if interrupts are enabled
 *
 * @return true if interrupts are enabled, false otherwise
 */
static inline bool is_intr_enabled(void) {
    uint64_t rflags;
    __asm__ volatile("pushfq; popq %0" : "=r"(rflags));
    return (rflags & 0x200) != 0;
}

/**
 * @brief Halt until the next interrupt
 */
static inline void interrupt_halt(void) {
    __asm__ volatile("hlt");
}

/* ============================================================================
 * Common interrupt handler (called from assembly stubs)
 * ============================================================================ */
void interrupt_handler(uint64_t vector, uint64_t error_code, interrupt_frame_t* frame);
