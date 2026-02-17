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
 * IDT Size and Interrupt Vector Numbers
 * ============================================================================ */

#define IDT_ENTRIES 256

// x86_64 Exception Vectors (0-31)
#define IDT_DE 0      // Divide Error (#DE)
#define IDT_DB 1      // Debug (#DB)
#define IDT_NMI 2     // Non-Maskable Interrupt (NMI)
#define IDT_BP 3      // Breakpoint (#BP)
#define IDT_OF 4      // Overflow (#OF)
#define IDT_BR 5      // BOUND Range Exceeded (#BR)
#define IDT_UD 6      // Invalid Opcode (#UD)
#define IDT_NM 7      // Device Not Available (#NM)
#define IDT_DF 8      // Double Fault (#DF)
#define IDT_CSO 9     // Coprocessor Segment Overrun (deprecated)
#define IDT_TS 10     // Invalid TSS (#TS)
#define IDT_NP 11     // Segment Not Present (#NP)
#define IDT_SS 12     // Stack-Segment Fault (#SS)
#define IDT_GP 13     // General Protection Fault (#GP)
#define IDT_PF 14     // Page Fault (#PF)
#define IDT_MF 15     // x87 FPU Error (#MF)
#define IDT_AC 16     // Alignment Check (#AC)
#define IDT_MC 17     // Machine Check (#MC)
#define IDT_XM 18     // SIMD Floating-Point Exception (#XM)
#define IDT_VE 19     // Virtualization Exception (#VE)
#define IDT_CP 20     // Control Protection Exception (#CP)
#define IDT_RSVD1 21  // Reserved
#define IDT_RSVD2 22  // Reserved
#define IDT_RSVD3 23  // Reserved
#define IDT_RSVD4 24  // Reserved
#define IDT_RSVD5 25  // Reserved
#define IDT_RSVD6 26  // Reserved
#define IDT_RSVD7 27  // Reserved
#define IDT_RSVD8 28  // Reserved
#define IDT_XF 29     // SSE Exception (#XF)
#define IDT_RSVD9 30  // Reserved
#define IDT_RSVD10 31 // Reserved

// IRQ Vectors (32-47) - after PIC remapping
#define IDT_IRQ_BASE 32
#define IDT_IRQ0 32  // Timer (PIC1)
#define IDT_IRQ1 33  // Keyboard
#define IDT_IRQ2 34  // Cascade (used internally by PIC)
#define IDT_IRQ3 35  // COM2
#define IDT_IRQ4 36  // COM1
#define IDT_IRQ5 37  // LPT2
#define IDT_IRQ6 38  // Floppy
#define IDT_IRQ7 39  // LPT1
#define IDT_IRQ8 40  // CMOS RTC (PIC2)
#define IDT_IRQ9 41  // Free for peripherals / SCSI / NIC
#define IDT_IRQ10 42 // Free for peripherals / SCSI / NIC
#define IDT_IRQ11 43 // Free for peripherals / SCSI / NIC
#define IDT_IRQ12 44 // PS/2 Mouse
#define IDT_IRQ13 45 // FPU / Coprocessor
#define IDT_IRQ14 46 // Primary ATA
#define IDT_IRQ15 47 // Secondary ATA

/* ============================================================================
 * IDT Type Attributes
 * ============================================================================ */

// Type field values (bits 0-3)
#define IDT_TYPE_TASK_GATE 0x5
#define IDT_TYPE_INTERRUPT_GATE 0xE
#define IDT_TYPE_TRAP_GATE 0xF

// Storage segment bit (bit 4) - must be 0 for interrupt gates
#define IDT_STORAGE_SEGMENT 0x0

// Descriptor Privilege Level (DPL, bits 5-6)
#define IDT_DPL_KERNEL 0x0 // Kernel level (DPL 0)
#define IDT_DPL_USER 0x3   // User level (DPL 3)

// Present bit (bit 7)
#define IDT_PRESENT 0x80
#define IDT_NOT_PRESENT 0x00

// Common attribute macros
#define IDT_KERNEL_INTERRUPT_GATE \
    (IDT_PRESENT | IDT_DPL_KERNEL | IDT_STORAGE_SEGMENT | IDT_TYPE_INTERRUPT_GATE)
#define IDT_USER_INTERRUPT_GATE \
    (IDT_PRESENT | IDT_DPL_USER | IDT_STORAGE_SEGMENT | IDT_TYPE_INTERRUPT_GATE)
#define IDT_KERNEL_TRAP_GATE \
    (IDT_PRESENT | IDT_DPL_KERNEL | IDT_STORAGE_SEGMENT | IDT_TYPE_TRAP_GATE)
#define IDT_USER_TRAP_GATE (IDT_PRESENT | IDT_DPL_USER | IDT_STORAGE_SEGMENT | IDT_TYPE_TRAP_GATE)

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
 * External Assembly Handler Declarations
 * ============================================================================ */

/* These are the assembly interrupt entry points defined in interrupt.s */
extern void isr0(void);  // Divide Error
extern void isr1(void);  // Debug
extern void isr2(void);  // Non-Maskable Interrupt
extern void isr3(void);  // Breakpoint
extern void isr4(void);  // Overflow
extern void isr5(void);  // BOUND Range Exceeded
extern void isr6(void);  // Invalid Opcode
extern void isr7(void);  // Device Not Available
extern void isr8(void);  // Double Fault
extern void isr9(void);  // Coprocessor Segment Overrun
extern void isr10(void); // Invalid TSS
extern void isr11(void); // Segment Not Present
extern void isr12(void); // Stack-Segment Fault
extern void isr13(void); // General Protection Fault
extern void isr14(void); // Page Fault
extern void isr15(void); // x87 FPU Error
extern void isr16(void); // Alignment Check
extern void isr17(void); // Machine Check
extern void isr18(void); // SIMD Floating-Point Exception
extern void isr19(void); // Virtualization Exception
extern void isr20(void); // Control Protection Exception
extern void isr21(void); // Reserved
extern void isr22(void); // Reserved
extern void isr23(void); // Reserved
extern void isr24(void); // Reserved
extern void isr25(void); // Reserved
extern void isr26(void); // Reserved
extern void isr27(void); // Reserved
extern void isr28(void); // Reserved
extern void isr29(void); // SSE Exception
extern void isr30(void); // Reserved
extern void isr31(void); // Reserved

/* IRQ handlers */
extern void irq0(void);  // Timer
extern void irq1(void);  // Keyboard
extern void irq2(void);  // Cascade
extern void irq3(void);  // COM2
extern void irq4(void);  // COM1
extern void irq5(void);  // LPT2
extern void irq6(void);  // Floppy
extern void irq7(void);  // LPT1
extern void irq8(void);  // RTC
extern void irq9(void);  // Free
extern void irq10(void); // Free
extern void irq11(void); // Free
extern void irq12(void); // PS/2 Mouse
extern void irq13(void); // FPU
extern void irq14(void); // Primary ATA
extern void irq15(void); // Secondary ATA

/* Common interrupt handler (called from assembly stubs) */
void interrupt_handler(uint64_t vector, uint64_t error_code, interrupt_frame_t* frame);
