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
 *
 * NOTE: This is the minimal CPU frame. For full context including general
 * purpose registers, see interrupt_stack_frame_t below.
 */
typedef struct PACKED {
    uint64_t error_code; // Error code (pushed only for some exceptions)
    uint64_t rip;        // Instruction pointer
    uint64_t cs;         // Code segment
    uint64_t rflags;     // RFLAGS register
    uint64_t rsp;        // Stack pointer
    uint64_t ss;         // Stack segment
} interrupt_frame_t;

/**
 * @brief Complete interrupt stack frame for iretq return
 *
 * This structure matches the exact stack layout in interrupt_common after
 * all registers are saved. It can be used to directly restore state and
 * return to user/kernel mode using iretq.
 *
 * Stack layout (from lower to higher addresses):
 *   CPU frame (pushed by CPU):
 *     [offset 176] error_code / dummy
 *     [offset 184] vector
 *     [offset 192] alignment
 *     [offset 200] RIP
 *     [offset 208] CS
 *     [offset 216] RFLAGS
 *     [offset 224] RSP (only if CPL changed)
 *     [offset 232] SS (only if CPL changed)
 *
 *   Saved by interrupt_common:
 *     [offset 0]  RAX
 *     [offset 8]  RBX
 *     [offset 16] RCX
 *     [offset 24] RDX
 *     [offset 32] RSI
 *     [offset 40] RDI
 *     [offset 48] RBP
 *     [offset 56] R8
 *     [offset 64] R9
 *     [offset 72] R10
 *     [offset 80] R11
 *     [offset 88] R12
 *     [offset 96] R13
 *     [offset 104] R14
 *     [offset 112] R15
 *     [offset 120] DS
 *     [offset 128] ES
 *     [offset 136] FS
 *     [offset 144] GS
 *
 * This structure allows direct manipulation of the interrupt stack for
 * purposes like modifying return values, changing execution path, or
 * implementing context switches.
 */
typedef struct PACKED {
    /* General purpose registers (saved by interrupt_common) */
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;

    /* Segment registers (saved by interrupt_common) */
    uint64_t ds;
    uint64_t es;
    uint64_t fs;
    uint64_t gs;

    /* CPU interrupt frame (pushed by CPU + stub) */
    uint64_t error_code;  /* Error code or dummy (stub/CPU push) */
    uint64_t vector;      /* Interrupt vector (stub push) */
    uint64_t alignment;   /* Alignment dummy (stub push) */
    uint64_t rip;         /* Instruction pointer (CPU push) */
    uint64_t cs;          /* Code segment (CPU push) */
    uint64_t rflags;      /* RFLAGS register (CPU push) */
    uint64_t rsp;         /* Stack pointer (CPU push, only if CPL changed) */
    uint64_t ss;          /* Stack segment (CPU push, only if CPL changed) */
} interrupt_stack_frame_t;

/* Compile-time checks to ensure structure size is correct */
_Static_assert(sizeof(interrupt_stack_frame_t) == 216UL,
               "interrupt_stack_frame_t must be exactly 216 bytes (27 * 8)");

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
 * @brief Set an IDT entry with IST
 *
 * @param vector Interrupt vector number (0-255)
 * @param handler Pointer to the interrupt handler function
 * @param type_attr Type attributes (e.g., IDT_KERNEL_INTERRUPT_GATE)
 * @param segment_selector Code segment selector (usually 0x08 for kernel code)
 * @param ist IST index (0-7, 0 = no IST switch)
 */
void idt_set_gate_ist(uint8_t vector, uint64_t handler, uint8_t type_attr,
                      uint16_t segment_selector, uint8_t ist);

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

/* ============================================================================
 * Direct iretq return from interrupt stack frame
 * ============================================================================ */

/**
 * @brief Restore interrupt stack frame and return using iretq
 *
 * This function is used to directly return from an interrupt by restoring
 * the complete interrupt stack frame and executing iretq. It's typically
 * called when:
 * 1. You want to modify the interrupt return context (e.g., change RIP)
 * 2. You're implementing custom context switching
 * 3. You need to return to a specific state
 *
 * @param frame Pointer to interrupt_stack_frame_t to restore
 *
 * NOTE: This function NEVER returns. It performs an iretq which transfers
 * control to the location specified in the frame.
 */
extern void interrupt_frame_iretq(interrupt_stack_frame_t* frame) __attribute__((noreturn));

/**
 * @brief Get current interrupt stack frame pointer
 *
 * This function returns a pointer to the current interrupt stack frame.
 * It can only be called from within an interrupt handler (when the
 * stack has the interrupt frame layout).
 *
 * WARNING: This is a low-level function that should only be used in
 * specific cases like:
 * - Implementing custom context switching
 * - Modifying interrupt return behavior
 * - Debugging interrupt handling
 *
 * @return Pointer to the current interrupt_stack_frame_t on the stack
 */
static inline interrupt_stack_frame_t* get_current_interrupt_frame(void) {
    interrupt_stack_frame_t* frame;
    __asm__ volatile(
        "mov %%rsp, %0"
        : "=r"(frame)
    );
    return frame;
}

/**
 * @brief Return directly from interrupt using iretq
 *
 * This function performs an immediate iretq from the current interrupt
 * context. It modifies the return context before executing iretq.
 *
 * @param new_rip If non-NULL, sets the return RIP to this value
 * @param new_rsp If non-NULL, sets the return RSP to this value
 *
 * NOTE: This function NEVER returns. Use with extreme caution!
 */
extern void interrupt_return_direct(uint64_t* new_rip, uint64_t* new_rsp) __attribute__((noreturn));
