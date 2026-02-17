/**
 * @file idt.c
 * @brief IDT implementation for x86_64
 */

#include "idt.h"
#include "base/memory.h"
#include "idt_constants.h"
#include "klogs/kprintf.h"

/* ============================================================================
 * IDT Entry Structure (implementation-private)
 * ============================================================================ */

/**
 * @brief IDT Entry Structure (16 bytes for x86_64)
 *
 * Format:
 * - Offset 0-1:  Handler address low 16 bits
 * - Offset 2-3:  Segment selector (code segment)
 * - Offset 4:    IST (Interrupt Stack Table) offset (bits 0-2) and zero (bits 3-7)
 * - Offset 5:    Type attributes (present, DPL, etc.)
 * - Offset 6-7:  Handler address middle 16 bits
 * - Offset 8-15: Handler address high 32 bits
 * - Offset 16-17: Reserved (must be 0)
 */
typedef struct PACKED {
    uint16_t offset_low;       // Lower 16 bits of handler address
    uint16_t segment_selector; // Code segment selector
    uint8_t ist;               // Interrupt Stack Table offset
    uint8_t type_attr;         // Type and attributes
    uint16_t offset_middle;    // Middle 16 bits of handler address
    uint32_t offset_high;      // Upper 32 bits of handler address
    uint32_t reserved;         // Reserved, must be 0
} idt_entry_t;

/**
 * @brief IDT Pointer Structure (used with lidt instruction)
 */
typedef struct PACKED {
    uint16_t limit; // Size of IDT - 1
    uint64_t base;  // Base address of IDT
} idt_ptr_t;

/* ============================================================================
 * Global IDT
 * ============================================================================ */

static idt_entry_t idt[IDT_ENTRIES];
static interrupt_handler_fn custom_handlers[IDT_ENTRIES] = {NULL};

/* ============================================================================
 * New IRQ Handler Table
 * ============================================================================ */

/**
 * @brief IRQ vector table entry
 */
typedef struct irq_vector_entry {
    irq_descriptor_t* descriptor; // Handler descriptor (NULL = no handler)
    bool in_use;                  // Whether this IRQ has a handler registered
} irq_vector_entry_t;

// IRQ handler table (16 IRQ lines)
static irq_vector_entry_t irq_table[16] = {0};

/* ============================================================================
 * IRQ Handler Adapter
 * ============================================================================ */

/**
 * @brief Adapter to bridge new irq_handler_fn to old interrupt_handler_fn
 *
 * The new API uses irq_handler_fn(frame, context) while the old
 * interrupt dispatcher uses interrupt_handler_fn(frame, error_code).
 * We store the new-style handlers and dispatch them correctly.
 */

// We'll directly call new-style handlers in interrupt_handler()
// No need for compatibility wrapper since we control both sides

/* ============================================================================
 * External Handler Tables (from interrupt.asm)
 * ============================================================================ */
extern void* const isr_handler_table[]; // Array of 32 ISR entry points
extern void* const irq_handler_table[]; // Array of 16 IRQ entry points

/* ============================================================================
 * Exception Name Table
 * ============================================================================ */

static const char* exception_names[] = {
    "Divide Error (#DE)",                  // 0
    "Debug (#DB)",                         // 1
    "Non-Maskable Interrupt (NMI)",        // 2
    "Breakpoint (#BP)",                    // 3
    "Overflow (#OF)",                      // 4
    "BOUND Range Exceeded (#BR)",          // 5
    "Invalid Opcode (#UD)",                // 6
    "Device Not Available (#NM)",          // 7
    "Double Fault (#DF)",                  // 8
    "Coprocessor Segment Overrun (#CSO)",  // 9
    "Invalid TSS (#TS)",                   // 10
    "Segment Not Present (#NP)",           // 11
    "Stack-Segment Fault (#SS)",           // 12
    "General Protection Fault (#GP)",      // 13
    "Page Fault (#PF)",                    // 14
    "x87 FPU Error (#MF)",                 // 15
    "Alignment Check (#AC)",               // 16
    "Machine Check (#MC)",                 // 17
    "SIMD Floating-Point Exception (#XM)", // 18
    "Virtualization Exception (#VE)",      // 19
    "Control Protection Exception (#CP)",  // 20
    "Reserved",                            // 21
    "Reserved",                            // 22
    "Reserved",                            // 23
    "Reserved",                            // 24
    "Reserved",                            // 25
    "Reserved",                            // 26
    "Reserved",                            // 27
    "Reserved",                            // 28
    "SSE Exception (#XF)",                 // 29
    "Reserved",                            // 30
    "Reserved"                             // 31
};

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

/**
 * @brief Set an IDT entry (internal implementation)
 */
static void idt_set_entry(uint8_t vector, uint64_t handler, uint8_t type_attr,
                          uint16_t segment_selector) {
    idt[vector].offset_low = (uint16_t)(handler & 0xFFFF);
    idt[vector].offset_middle = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].segment_selector = segment_selector;
    idt[vector].ist = 0; // No IST for now
    idt[vector].type_attr = type_attr;
    idt[vector].reserved = 0;
}

/* ============================================================================
 * Public Functions
 * ============================================================================ */

void idt_init(void) {
    // Clear the entire IDT
    memset(idt, 0, sizeof(idt));

    // Setup IDT pointer
    idt_ptr_t idt_ptr;
    idt_ptr.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idt_ptr.base = (uint64_t)&idt;

    // Kernel code segment selector (should match your GDT)
    // Bootloader GDT: gdt_code64 is at offset 0x18 (3rd selector)
    uint16_t kernel_cs = 0x18;

    // Set up exception handlers (ISRs 0-31) using loop
    for (int i = 0; i < 32; i++) {
        // Breakpoint (#BP) and Overflow (#OF) use trap gates
        uint8_t type =
            (i == IDT_BP || i == IDT_OF) ? IDT_KERNEL_TRAP_GATE : IDT_KERNEL_INTERRUPT_GATE;
        idt_set_entry(i, (uint64_t)isr_handler_table[i], type, kernel_cs);
    }

    // Set up IRQ handlers (IRQs 0-15 -> vectors 32-47) using loop
    for (int i = 0; i < 16; i++) {
        idt_set_entry(IDT_IRQ_BASE + i, (uint64_t)irq_handler_table[i], IDT_KERNEL_INTERRUPT_GATE,
                      kernel_cs);
    }

    // Load the IDT
    idt_load((uint64_t)&idt_ptr);

    klog_trace("IDT initialized with %d entries at 0x%016lx\n", IDT_ENTRIES, (uint64_t)&idt);
}

void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t type_attr, uint16_t segment_selector) {
    idt[vector].offset_low = (uint16_t)(handler & 0xFFFF);
    idt[vector].offset_middle = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].segment_selector = segment_selector;
    idt[vector].ist = 0;
    idt[vector].type_attr = type_attr;
    idt[vector].reserved = 0;
}

void idt_register_handler(uint8_t vector, interrupt_handler_fn handler) {
    custom_handlers[vector] = handler;
}

const char* idt_get_exception_name(uint8_t vector) {
    if (vector < 32) {
        return exception_names[vector];
    }
    return "Unknown";
}

/**
 * @brief Register an IRQ handler with descriptor
 */
int irq_register_handler(uint8_t irq, irq_descriptor_t* descriptor) {
    if (irq >= 16) {
        klog_error("Invalid IRQ number: %d (must be 0-15)\n", irq);
        return -1;
    }

    if (descriptor == NULL) {
        klog_error("NULL descriptor for IRQ %d\n", irq);
        return -2;
    }

    if (descriptor->handler == NULL) {
        klog_error("NULL handler in descriptor for IRQ %d\n", irq);
        return -3;
    }

    irq_vector_entry_t* entry = &irq_table[irq];

    // Check if IRQ already has a handler (simplified: no shared IRQ support yet)
    if (entry->in_use && entry->descriptor != NULL) {
        klog_error("IRQ %d already has a handler registered (%s)\n", irq, entry->descriptor->name);
        return -4;
    }

    // Register the handler
    entry->descriptor = descriptor;
    entry->in_use = true;

    // Note: We don't use custom_handlers[] for new-style IRQ handlers anymore.
    // The interrupt_handler() function now checks irq_table[] first for IRQs.
    klog_trace("Registered IRQ %d handler: %s\n", irq,
               descriptor->name ? descriptor->name : "unnamed");

    return 0;
}

/**
 * @brief Unregister an IRQ handler
 */
int irq_unregister_handler(uint8_t irq, irq_descriptor_t* descriptor) {
    if (irq >= 16) {
        return -1;
    }

    irq_vector_entry_t* entry = &irq_table[irq];

    if (entry->descriptor != descriptor) {
        klog_error("Descriptor mismatch for IRQ %d\n", irq);
        return -2;
    }

    // Clear the entry
    entry->descriptor = NULL;
    entry->in_use = false;

    klog_trace("Unregistered IRQ %d handler: %s\n", irq,
               descriptor->name ? descriptor->name : "unnamed");

    return 0;
}

/* ============================================================================
 * Common Interrupt Handler
 * ============================================================================ */

/**
 * @brief Default exception handler for unhandled exceptions
 */
static void default_exception_handler(interrupt_frame_t* frame, uint64_t vector,
                                      uint64_t error_code) {
    klog_error("\n");
    klog_error("=== EXCEPTION OCCURRED ===\n");
    klog_error("Vector: %d - %s\n", vector, idt_get_exception_name(vector));
    klog_error("Error Code: 0x%016lx\n", error_code);
    klog_error("RIP: 0x%016lx\n", frame->rip);
    klog_error("CS:  0x%016lx\n", frame->cs);
    klog_error("RFLAGS: 0x%016lx\n", frame->rflags);
    klog_error("RSP: 0x%016lx\n", frame->rsp);
    klog_error("SS:  0x%016lx\n", frame->ss);
    klog_error("==========================\n");

    // Halt the system on fatal exceptions
    klog_error("System halted due to exception.\n");
    interrupt_disable();
    while (1) {
        interrupt_halt();
    }
}

void interrupt_handler(uint64_t vector, uint64_t error_code, interrupt_frame_t* frame) {
    (void)error_code;
    extern void pic_send_eoi(uint8_t irq);

    if (vector < 32) {
        // CPU exceptions - use default exception handler
        // Note: Exception handlers use the old interrupt_handler_fn signature
        if (custom_handlers[vector] != NULL) {
            custom_handlers[vector](frame, error_code);
        } else {
            default_exception_handler(frame, vector, error_code);
        }
    } else if (vector >= 32 && vector < 48) {
        // IRQ interrupt
        uint8_t irq = vector - 32;
        irq_vector_entry_t* entry = &irq_table[irq];

        if (entry->in_use && entry->descriptor != NULL) {
            // Call new-style IRQ handler
            irq_descriptor_t* desc = entry->descriptor;
            desc->invocation_count++;
            desc->handler(frame, desc->context);

            // Check if handler sends EOI automatically
            if (!(desc->flags & IRQ_FLAG_AUTOEOI)) {
                pic_send_eoi(irq); // Marking as Handled Already
            }
        } else if (custom_handlers[vector] != NULL) {
            // Fallback to old-style handler for compatibility
            custom_handlers[vector](frame, error_code);
            pic_send_eoi(irq);
        } else {
            klog_warn("Meeting One IRQ: %d occurred but no handler registered, that might be "
                      "unexpected...",
                      irq);
            pic_send_eoi(irq);
        }
    } else {
        // Spurious interrupt or unexpected interrupt
        klog_warn("Meeting Spurious interrupt: vector %d\n", vector);
        extern void pic_send_eoi(uint8_t irq);
        pic_send_eoi(7); // Send EOI for IRQ 7 (common spurious interrupt)
    }
}
