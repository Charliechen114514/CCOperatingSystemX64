/* ==============================================================================
 * CCOS - User Mode Support
 * ==============================================================================
 * This module provides user mode (Ring 3) support including:
 * - User mode process creation and Ring 3 transition
 * - Safe memory access between user and kernel space
 * - User memory management (heap, mmap, etc.)
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"
#include "process/process.h"
#include "mm/vmm/vmm_config.h"
#include "mm/page_config.h"

/* ==============================================================================
 * User Mode Constants
 * ============================================================================== */

/* User stack configuration */
#define USER_STACK_SIZE    (1 * 1024 * 1024)   /* 1MB user stack */
#define USER_STACK_TOP     (USER_END - 8)       /* Top of user stack (aligned) */

/* User mode segment selectors */
#define USER_CS    (0x18 | 3)   /* GDT_USER_CODE with RPL=3 */
#define USER_SS    (0x20 | 3)   /* GDT_USER_DATA with RPL=3 */

/* ==============================================================================
 * User Memory Region Types
 * ============================================================================== */

typedef enum user_region_type {
    USER_REGION_TEXT,      /* Executable code */
    USER_REGION_DATA,      /* Read-write data */
    USER_REGION_RODATA,    /* Read-only data */
    USER_REGION_HEAP,      /* Heap (grows upward) */
    USER_REGION_STACK,     /* Stack (grows downward) */
    USER_REGION_MMAP,      /* Memory-mapped region */
} user_region_type_t;

/* ==============================================================================
 * User Context Structure
 * ============================================================================== */

/**
 * @brief User context for Ring 3 transition
 *
 * This structure contains the information needed to switch from
 * kernel mode (Ring 0) to user mode (Ring 3).
 */
typedef struct user_context {
    virtual_addr_t entry;      /* Entry point (user RIP) */
    virtual_addr_t stack_top;  /* Top of user stack (user RSP) */
    uint64_t cs;               /* User CS selector (USER_CS) */
    uint64_t ss;               /* User SS selector (USER_SS) */
    uint64_t rflags;           /* User RFLAGS (IF enabled) */
} user_context_t;

/* ==============================================================================
 * User Memory Region Descriptor
 * ============================================================================== */

/**
 * @brief User memory region descriptor
 */
typedef struct user_region {
    virtual_addr_t start;           /* Region start */
    virtual_addr_t end;             /* Region end */
    physical_addr_t phys_start;     /* Physical address (0 if anon) */
    uint64_t flags;                 /* Protection flags */
    user_region_type_t type;        /* Region type */
    char name[32];                  /* Region name for debugging */
} user_region_t;

/* ==============================================================================
 * User Mode Process Creation API
 * ============================================================================== */

/**
 * @brief Create a new user mode process
 * @param entry Entry point address
 * @param pcb Pointer to PCB to initialize
 * @return 0 on success, negative on error
 */
int user_create_process(virtual_addr_t entry, pcb_t* pcb);

/**
 * @brief Destroy a user mode process and free its resources
 * @param pcb Process PCB to destroy
 */
void user_destroy_process(pcb_t* pcb);

/**
 * @brief Set up user stack for a process
 * @param pcb Process PCB
 * @param argv Argument vector (NULL for none)
 * @param envp Environment vector (NULL for none)
 * @return 0 on success, negative on error
 */
int user_setup_user_stack(pcb_t* pcb, const char** argv, const char** envp);

/**
 * @brief Execute transition from Ring 0 to Ring 3
 *
 * This function does NOT return. It loads the user context and
 * executes an iretq to switch to user mode.
 *
 * @param ctx User context to load
 */
void user_switch_to_usermode(user_context_t* ctx) __attribute__((noreturn));

/* ==============================================================================
 * Safe User Memory Access API
 * ============================================================================== */

/**
 * @brief Check if a user pointer is valid
 * @param ptr Pointer to validate
 * @param size Size of access
 * @param write True if write access needed
 * @return true if pointer is valid and accessible
 */
bool user_validate_pointer(const void* ptr, size_t size, bool write);

/**
 * @brief Safely copy data from user space
 * @param dst Kernel destination buffer
 * @param src User source buffer
 * @param count Number of bytes to copy
 * @return Number of bytes copied, or negative on error
 */
int64_t user_copy_from_user(void* dst, const void* src, size_t count);

/**
 * @brief Safely copy data to user space
 * @param dst User destination buffer
 * @param src Kernel source buffer
 * @param count Number of bytes to copy
 * @return Number of bytes copied, or negative on error
 */
int64_t user_copy_to_user(void* dst, const void* src, size_t count);

/**
 * @brief Safely copy string from user space
 * @param dst Kernel destination buffer
 * @param src User source string
 * @param max_len Maximum length to copy
 * @return Number of bytes copied (excluding null), or negative on error
 */
int64_t user_strncpy_from_user(char* dst, const char* src, size_t max_len);

/**
 * @brief Check if user string is valid
 * @param str User string to validate
 * @param max_len Maximum valid length
 * @return true if string is valid and null-terminated within bounds
 */
bool user_validate_string(const char* str, size_t max_len);

/* ==============================================================================
 * User Memory Region Management
 * ============================================================================== */

/**
 * @brief Map a memory region into user space
 * @param pcb Process PCB
 * @param vaddr Virtual address (0 for auto-allocate)
 * @param size Size of region (must be page-aligned)
 * @param flags Protection flags (VMAP_FLAG_*)
 * @param type Region type
 * @param name Region name for debugging
 * @return Mapped virtual address, or 0 on error
 */
virtual_addr_t user_map_region(pcb_t* pcb, virtual_addr_t vaddr, size_t size,
                               uint64_t flags, user_region_type_t type, const char* name);

/**
 * @brief Unmap a memory region from user space
 * @param pcb Process PCB
 * @param vaddr Virtual address to unmap
 * @param size Size of region
 * @return 0 on success, negative on error
 */
int user_unmap_region(pcb_t* pcb, virtual_addr_t vaddr, size_t size);

/**
 * @brief Find a free virtual address range
 * @param pcb Process PCB
 * @param min_addr Minimum acceptable address
 * @param max_addr Maximum acceptable address
 * @param size Size needed
 * @return Found virtual address, or 0 on error
 */
virtual_addr_t user_find_free_region(pcb_t* pcb, virtual_addr_t min_addr,
                                     virtual_addr_t max_addr, size_t size);

/* ==============================================================================
 * Heap Management API
 * ============================================================================== */

/**
 * @brief Set program break (traditional brk syscall)
 * @param pcb Process PCB
 * @param new_brk New program break
 * @return New program break, or 0 on error
 */
virtual_addr_t user_brk(pcb_t* pcb, virtual_addr_t new_brk);

/**
 * @brief Map memory into user space (mmap syscall)
 * @param pcb Process PCB
 * @param addr Suggested address (0 for any)
 * @param length Length of mapping
 * @param prot Protection flags
 * @param flags Mapping flags
 * @param fd File descriptor (0 for anonymous)
 * @param offset File offset
 * @return Mapped address, or 0 on error
 */
virtual_addr_t user_mmap(pcb_t* pcb, virtual_addr_t addr, size_t length,
                        int prot, int flags, int fd, size_t offset);

/**
 * @brief Unmap memory from user space
 * @param pcb Process PCB
 * @param addr Address to unmap
 * @param length Length of region
 * @return 0 on success, negative on error
 */
int user_munmap(pcb_t* pcb, virtual_addr_t addr, size_t length);

/* ==============================================================================
 * User Copy Fault Handling
 * ============================================================================== */

/**
 * @brief Check if a page fault occurred during user copy
 * @return true if a copy operation faulted
 */
bool user_copy_faulted(void);

/**
 * @brief Set up user copy fault detection
 */
void user_copy_begin(void);

/**
 * @brief Clean up user copy fault detection
 * @return true if a fault occurred during the copy
 */
bool user_copy_end(void);

/* ==============================================================================
 * Initialization
 * ============================================================================== */

/**
 * @brief Initialize user mode subsystem
 * @return 0 on success, negative on error
 */
int user_init(void);
