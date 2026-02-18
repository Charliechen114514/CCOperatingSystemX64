/* ==============================================================================
 * CCOS - Page Fault Handler
 * ==============================================================================
 * This module handles x86_64 page fault exceptions (vector 14, #PF),
 * including parsing error codes, accessing CR2 register, and implementing
 * copy-on-write and demand paging mechanisms.
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"
#include "interrupt/idt.h"
#include "interrupt/idt_constants.h"  /* For IDT_PF */

/* Forward declaration for vmm_result_t to avoid circular dependency
 * Note: This is defined in vmm.h, but we need it here for function signatures.
 * We use a conditional to avoid redefinition errors.
 */
#ifndef VMM_RESULT_T_DEFINED
#define VMM_RESULT_T_DEFINED
typedef enum {
    VMM_OK = 0,
    VMM_ERR_NOT_INIT = -1,
    VMM_ERR_OOM = -2,
    VMM_ERR_INVALID = -3,
    VMM_ERR_PERM = -4,
    VMM_ERR_NOT_MAPPED = -5,
    VMM_ERR_ALREADY_MAPPED = -6,
} vmm_result_t;
#endif

/* ==============================================================================
 * Page Fault Error Code Bits
 * ==============================================================================
 *
 * The page fault error code is pushed by the CPU when a #PF occurs.
 * It provides information about the cause of the fault.
 */

#define PF_ERR_PRESENT   (1 << 0)  /* Bit 0: P=0 if page not present, P=1 if protection fault */
#define PF_ERR_WRITE     (1 << 1)  /* Bit 1: W=1 if write operation, W=0 if read */
#define PF_ERR_USER      (1 << 2)  /* Bit 2: U=1 if user mode, U=0 if supervisor mode */
#define PF_ERR_RESERVED  (1 << 3)  /* Bit 3: Reserved bit set in page table */
#define PF_ERR_INSTR     (1 << 4)  /* Bit 4: I=1 if instruction fetch, I=0 if data access */

/* ==============================================================================
 * Page Fault Information Structure
 * ============================================================================== */

/**
 * @brief Parsed page fault information
 *
 * This structure contains the parsed information from a page fault,
 * including the faulting address and the cause of the fault.
 */
typedef struct {
    virtual_addr_t fault_addr;      /* CR2: Faulting virtual address */
    uint64_t error_code;            /* Raw error code from CPU */
    bool present;                   /* Was the page present? */
    bool write;                     /* Was it a write operation? */
    bool user;                      /* Did it occur in user mode? */
    bool reserved_bit;              /* Was a reserved bit set? */
    bool instruction_fetch;         /* Was it an instruction fetch? */
} page_fault_info_t;

/* ==============================================================================
 * Page Fault Handler Result Codes
 * ============================================================================== */

typedef enum {
    PF_SUCCESS,              /* Fault was handled successfully */
    PF_NOT_OUR_FAULT,        /* Not a fault we should handle */
    PF_OOM,                  /* Out of memory */
    PF_ACCESS_DENIED,        /* Access violation */
    PF_INVALID_ADDRESS,      /* Invalid address */
    PF_KERNEL_PANIC,         /* Kernel should panic */
} pf_result_t;

/* ==============================================================================
 * Page Fault Handler API
 * ============================================================================ */

/**
 * pf_init - Initialize the page fault handler
 *
 * Registers the page fault handler with the IDT for vector 14.
 * Should be called after vmm_init().
 */
void pf_init(void);

/**
 * pf_handler - Page fault interrupt handler
 *
 * Called by the interrupt system when a page fault (vector 14) occurs.
 * Parses the error code, reads CR2 for the fault address, and attempts
 * to handle the fault.
 *
 * @param frame Interrupt stack frame
 * @param error_code Page fault error code pushed by CPU
 */
void pf_handler(interrupt_frame_t* frame, uint64_t error_code);

/**
 * pf_parse_error_code - Parse page fault error code
 *
 * Extracts individual bits from the page fault error code into
 * a more readable structure.
 *
 * @param error_code Raw error code from CPU
 * @param info Pointer to structure to fill with parsed info
 */
void pf_parse_error_code(uint64_t error_code, page_fault_info_t* info);

/**
 * pf_get_cr2 - Read CR2 register (fault address)
 *
 * @return Virtual address that caused the page fault
 */
static inline virtual_addr_t pf_get_cr2(void) {
    virtual_addr_t cr2;
    __asm__ volatile("movq %%cr2, %0" : "=r"(cr2));
    return cr2;
}

/* ============================================================================
 * Copy-on-Write Support
 * ============================================================================ */

/**
 * @brief Forward declarations - full definitions in cow.h
 */
typedef struct cow_block cow_block_t;
typedef struct cow_region cow_region_t;

/**
 * pf_handle_cow - Handle a copy-on-write page fault
 *
 * Called when a write fault occurs on a COW page. Delegates to the
 * COW module for actual handling.
 *
 * @param pml4 Current address space PML4
 * @param fault_addr Address that caused the fault
 * @return PF_SUCCESS if handled, error code otherwise
 */
pf_result_t pf_handle_cow(physical_addr_t pml4, virtual_addr_t fault_addr);

/**
 * pf_register_cow_region - Register a COW region
 *
 * Marks a region of memory as copy-on-write. Delegates to the COW module.
 *
 * @param pml4 Address space PML4
 * @param base Base address of region
 * @param size Size of region in bytes
 * @return VMM_OK on success, error code otherwise
 */
vmm_result_t pf_register_cow_region(physical_addr_t pml4, virtual_addr_t base, size_t size);

/* ============================================================================
 * Demand Paging Support (Future Enhancement)
 * ============================================================================ */

/**
 * pf_handle_demand_page - Handle a demand page fault
 *
 * Called when a page is accessed but not yet mapped. Allocates a
 * physical page and maps it to the fault address.
 *
 * @param fault_addr Address that caused the fault
 * @param user True if fault occurred in user mode
 * @return PF_SUCCESS if handled, error code otherwise
 */
pf_result_t pf_handle_demand_page(virtual_addr_t fault_addr, bool user);
