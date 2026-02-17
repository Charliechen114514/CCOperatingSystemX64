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
