/**
 * @file idt_constants.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief place some idt contants seperately
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once
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
#define IDT_NOT_PRESENT 0x00 // Common attribute macros
#define IDT_KERNEL_INTERRUPT_GATE \
    (IDT_PRESENT | IDT_DPL_KERNEL | IDT_STORAGE_SEGMENT | IDT_TYPE_INTERRUPT_GATE)
#define IDT_USER_INTERRUPT_GATE \
    (IDT_PRESENT | IDT_DPL_USER | IDT_STORAGE_SEGMENT | IDT_TYPE_INTERRUPT_GATE)
#define IDT_KERNEL_TRAP_GATE \
    (IDT_PRESENT | IDT_DPL_KERNEL | IDT_STORAGE_SEGMENT | IDT_TYPE_TRAP_GATE)
#define IDT_USER_TRAP_GATE (IDT_PRESENT | IDT_DPL_USER | IDT_STORAGE_SEGMENT | IDT_TYPE_TRAP_GATE)
