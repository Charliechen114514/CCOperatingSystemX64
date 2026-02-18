/* ==============================================================================
 * CCOS - User Mode Support Implementation
 * ==============================================================================
 */

#include "user/user.h"
#include "mm/vmm/vmm.h"
#include "mm/pframe/pframe.h"
#include "mm/vmm/page.h"
#include "process/process.h"
#include "process/process_defines.h"
#include "mm/heap/heap.h"
#include "klogs/kprintf.h"
#include "base/memory.h"

/* ==============================================================================
 * User Copy Fault Handling
 * ============================================================================== */

/* State for page fault fixup during user copy operations */
static volatile bool s_copy_in_progress = false;
static volatile bool s_copy_faulted = false;

bool user_copy_faulted(void) {
    return s_copy_faulted;
}

void user_copy_begin(void) {
    s_copy_in_progress = true;
    s_copy_faulted = false;

    /* Register our temporary page fault handler */
    /* Note: This is a simplified version. A full implementation would
     * properly save/restore the handler and handle nested calls. */
    /* For now, we'll use a simpler approach with validation only */
}

bool user_copy_end(void) {
    bool faulted = s_copy_faulted;
    s_copy_in_progress = false;
    s_copy_faulted = false;
    return faulted;
}

/* ==============================================================================
 * Safe User Memory Access
 * ============================================================================== */

bool user_validate_pointer(const void* ptr, size_t size, bool write) {
    (void)write;  /* TODO: Use write flag to check permissions */

    virtual_addr_t addr = (virtual_addr_t)ptr;

    /* Check if address is in user space */
    if (!vmm_is_user_addr(addr)) {
        return false;
    }

    /* Check if the entire range fits in user space */
    if (addr + size < addr) {  /* Overflow check */
        return false;
    }

    if (addr + size > USER_END) {
        return false;
    }

    /* Check for NULL page access (first 4MB is protected) */
    if (addr < USER_BASE) {
        return false;
    }

    /* TODO: Add actual page table walk to verify pages are mapped
     * and have correct permissions. For now, we just check ranges. */

    return true;
}

int64_t user_copy_from_user(void* dst, const void* src, size_t count) {
    if (count == 0) {
        return 0;
    }

    /* Validate source is in user space */
    if (!user_validate_pointer(src, count, false)) {
        klog_warn("[USER] Invalid user pointer in copy_from_user: 0x%llX\n", src);
        return -1;
    }

    /* For now, just do a direct copy. A full implementation would
     * use page fault fixup to handle unmapped pages gracefully. */
    memcpy(dst, src, count);
    return (int64_t)count;
}

int64_t user_copy_to_user(void* dst, const void* src, size_t count) {
    if (count == 0) {
        return 0;
    }

    /* Validate destination is in user space */
    if (!user_validate_pointer(dst, count, true)) {
        klog_warn("[USER] Invalid user pointer in copy_to_user: 0x%llX\n", dst);
        return -1;
    }

    /* For now, just do a direct copy */
    memcpy(dst, src, count);
    return (int64_t)count;
}

int64_t user_strncpy_from_user(char* dst, const char* src, size_t max_len) {
    if (max_len == 0) {
        return 0;
    }

    /* Validate source is in user space */
    if (!user_validate_pointer(src, max_len, false)) {
        klog_warn("[USER] Invalid user string pointer: 0x%llX\n", src);
        return -1;
    }

    /* Copy string, ensuring we don't read past max_len */
    size_t i;
    for (i = 0; i < max_len; i++) {
        dst[i] = src[i];
        if (src[i] == '\0') {
            return (int64_t)i;  /* Return length excluding null */
        }
    }

    /* String wasn't null-terminated within max_len */
    dst[max_len - 1] = '\0';
    return (int64_t)max_len;
}

bool user_validate_string(const char* str, size_t max_len) {
    if (!user_validate_pointer(str, max_len, false)) {
        return false;
    }

    /* Check if string is null-terminated within max_len */
    for (size_t i = 0; i < max_len; i++) {
        if (str[i] == '\0') {
            return true;
        }
    }

    return false;
}

/* ==============================================================================
 * User Mode Process Creation
 * ============================================================================ */

int user_create_process(virtual_addr_t entry, pcb_t* pcb) {
    if (!pcb) {
        return -1;
    }

    (void)entry;  /* TODO: Use entry point when implementing exec */

    klog_info("[USER] Creating user process\n");

    /* Allocate user stack - ensure page alignment */
    size_t stack_pages = USER_STACK_SIZE / PAGE_SIZE;
    pcb->user_stack_size = USER_STACK_SIZE;
    /* Align user stack to page boundary (USER_END may not be page-aligned) */
    pcb->user_stack = (USER_END - USER_STACK_SIZE) & ~(PAGE_SIZE - 1);

    /* Map user stack pages */
    for (size_t i = 0; i < stack_pages; i++) {
        physical_addr_t paddr;
        if (pframe_alloc(&paddr) != PFRAME_OK) {
            klog_error("[USER] Failed to allocate user stack page\n");
            /* Cleanup already allocated pages */
            for (size_t j = 0; j < i; j++) {
                virtual_addr_t vaddr = pcb->user_stack + (j * PAGE_SIZE);
                physical_addr_t phys;
                if (page_virt_to_phys(pcb->mm.pml4_phys, vaddr, &phys) == PAGE_OK) {
                    pframe_free(phys);
                }
                page_unmap_page(pcb->mm.pml4_phys, vaddr, false);
            }
            pcb->user_stack = 0;
            pcb->user_stack_size = 0;
            return -1;
        }

        virtual_addr_t vaddr = pcb->user_stack + (i * PAGE_SIZE);
        vmm_result_t result = vmm_map_to_user(pcb->mm.pml4_phys, vaddr, paddr, 1,
                                               VMAP_FLAG_WRITE | VMAP_FLAG_USER);
        if (result != VMM_OK) {
            klog_error("[USER] Failed to map user stack page\n");
            pframe_free(paddr);
            /* Cleanup */
            for (size_t j = 0; j < i; j++) {
                virtual_addr_t vaddr2 = pcb->user_stack + (j * PAGE_SIZE);
                physical_addr_t phys;
                if (page_virt_to_phys(pcb->mm.pml4_phys, vaddr2, &phys) == PAGE_OK) {
                    pframe_free(phys);
                }
                page_unmap_page(pcb->mm.pml4_phys, vaddr2, false);
            }
            pcb->user_stack = 0;
            pcb->user_stack_size = 0;
            return -1;
        }

        /* Clear the stack page */
        memset((void*)phys_to_virt_offset(paddr), 0, PAGE_SIZE);
    }

    pcb->is_user_mode = true;

    klog_info("[USER] User process created: stack=0x%llX, stack_size=%zu\n",
              pcb->user_stack, pcb->user_stack_size);

    return 0;
}

void user_destroy_process(pcb_t* pcb) {
    if (!pcb || !pcb->is_user_mode) {
        return;
    }

    klog_info("[USER] Destroying user process\n");

    /* Free user stack pages */
    if (pcb->user_stack != 0 && pcb->user_stack_size != 0) {
        size_t stack_pages = pcb->user_stack_size / PAGE_SIZE;
        for (size_t i = 0; i < stack_pages; i++) {
            virtual_addr_t vaddr = pcb->user_stack + (i * PAGE_SIZE);
            physical_addr_t phys;
            if (page_virt_to_phys(pcb->mm.pml4_phys, vaddr, &phys) == PAGE_OK) {
                pframe_free(phys);
            }
            page_unmap_page(pcb->mm.pml4_phys, vaddr, false);
        }
    }

    /* Destroy the user address space (frees page tables) */
    if (pcb->mm.pml4_phys != 0) {
        vmm_destroy_user_space(pcb->mm.pml4_phys);
    }

    klog_info("[USER] User process destroyed\n");
}

int user_setup_user_stack(pcb_t* pcb, const char** argv, const char** envp) {
    if (!pcb || !pcb->is_user_mode) {
        return -1;
    }

    /* TODO: Implement proper argv/envp setup on user stack
     * For now, we just set up a minimal stack */
    (void)argv;
    (void)envp;

    return 0;
}

/* ==============================================================================
 * User Memory Region Management
 * ============================================================================ */

#define MAX_USER_REGIONS  64

typedef struct user_mm_state {
    user_region_t regions[MAX_USER_REGIONS];
    uint32_t region_count;
    virtual_addr_t heap_start;
    virtual_addr_t heap_end;
} user_mm_state_t;

/* Note: In a real implementation, this would be per-process.
 * For now, we use a simple global approach. */

virtual_addr_t user_map_region(pcb_t* pcb, virtual_addr_t vaddr, size_t size,
                               uint64_t flags, user_region_type_t type, const char* name) {
    (void)type;   /* TODO: Use type for region tracking */
    (void)name;   /* TODO: Use name for debugging */
    if (!pcb) {
        return 0;
    }

    /* Align size to page boundary */
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (size == 0) {
        return 0;
    }

    /* If vaddr is 0, find a free region */
    if (vaddr == 0) {
        vaddr = user_find_free_region(pcb, USER_BASE, USER_END, size);
        if (vaddr == 0) {
            return 0;
        }
    } else {
        /* Align vaddr to page boundary */
        vaddr = vaddr & ~(PAGE_SIZE - 1);
    }

    /* Allocate and map pages */
    for (size_t i = 0; i < size / PAGE_SIZE; i++) {
        physical_addr_t paddr;
        if (pframe_alloc(&paddr) != PFRAME_OK) {
            /* Rollback */
            user_unmap_region(pcb, vaddr, i * PAGE_SIZE);
            return 0;
        }

        vmm_result_t result = vmm_map_to_user(pcb->mm.pml4_phys,
                                               vaddr + (i * PAGE_SIZE),
                                               paddr, 1,
                                               flags | VMAP_FLAG_USER);
        if (result != VMM_OK) {
            pframe_free(paddr);
            user_unmap_region(pcb, vaddr, i * PAGE_SIZE);
            return 0;
        }

        /* Clear the page for anonymous mappings */
        memset((void*)phys_to_virt_offset(paddr), 0, PAGE_SIZE);
    }

    klog_trace("[USER] Mapped region: 0x%llX - 0x%llX (%s)\n",
               vaddr, vaddr + size, name ? name : "anonymous");

    return vaddr;
}

int user_unmap_region(pcb_t* pcb, virtual_addr_t vaddr, size_t size) {
    if (!pcb) {
        return -1;
    }

    /* Align to page boundary */
    vaddr = vaddr & ~(PAGE_SIZE - 1);
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    /* Unmap pages and free physical frames */
    for (size_t i = 0; i < size / PAGE_SIZE; i++) {
        virtual_addr_t addr = vaddr + (i * PAGE_SIZE);

        /* Get physical address */
        physical_addr_t paddr;
        if (page_virt_to_phys(pcb->mm.pml4_phys, addr, &paddr) == PAGE_OK) {
            pframe_free(paddr);
        }

        /* Unmap the page */
        page_unmap_page(pcb->mm.pml4_phys, addr, false);
    }

    return 0;
}

virtual_addr_t user_find_free_region(pcb_t* pcb, virtual_addr_t min_addr,
                                     virtual_addr_t max_addr, size_t size) {
    (void)pcb;  /* TODO: Use per-process region tracking */

    /* For now, use a simple linear search from min_addr */
    virtual_addr_t addr = min_addr;

    /* Align to page boundary */
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    addr = (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    /* Simple search - just return addr if it fits */
    /* TODO: Implement proper free region tracking */
    if (addr + size <= max_addr) {
        return addr;
    }

    return 0;
}

/* ==============================================================================
 * Heap Management
 * ============================================================================ */

virtual_addr_t user_brk(pcb_t* pcb, virtual_addr_t new_brk) {
    if (!pcb) {
        return 0;
    }

    /* NULL argument queries current break */
    if (new_brk == 0) {
        return pcb->mm.brk;
    }

    /* Align to page boundary */
    new_brk = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    /* Check if new break is valid */
    if (new_brk < USER_BASE || new_brk >= (USER_END - USER_STACK_SIZE)) {
        return pcb->mm.brk;  /* Return old break on error */
    }

    /* For shrinking, just update the break */
    if (new_brk <= pcb->mm.brk) {
        /* Optional: unmap pages that are no longer needed */
        pcb->mm.brk = new_brk;
        return new_brk;
    }

    /* For expanding, allocate new pages */
    size_t old_brk = pcb->mm.brk;
    if (old_brk == 0) {
        old_brk = USER_BASE + (16 * 1024 * 1024);  /* Start heap at 16MB */
    }

    size_t additional_pages = (new_brk - old_brk) / PAGE_SIZE;

    for (size_t i = 0; i < additional_pages; i++) {
        physical_addr_t paddr;
        if (pframe_alloc(&paddr) != PFRAME_OK) {
            /* Failed, return old break */
            return pcb->mm.brk ? pcb->mm.brk : old_brk;
        }

        virtual_addr_t vaddr = old_brk + (i * PAGE_SIZE);
        vmm_result_t result = vmm_map_to_user(pcb->mm.pml4_phys, vaddr,
                                               paddr, 1,
                                               VMAP_FLAG_WRITE | VMAP_FLAG_USER);
        if (result != VMM_OK) {
            pframe_free(paddr);
            /* Rollback */
            for (size_t j = 0; j < i; j++) {
                virtual_addr_t vaddr2 = old_brk + (j * PAGE_SIZE);
                physical_addr_t phys;
                if (page_virt_to_phys(pcb->mm.pml4_phys, vaddr2, &phys) == PAGE_OK) {
                    pframe_free(phys);
                }
                page_unmap_page(pcb->mm.pml4_phys, vaddr2, false);
            }
            return pcb->mm.brk ? pcb->mm.brk : old_brk;
        }

        /* Clear the new page */
        memset((void*)phys_to_virt_offset(paddr), 0, PAGE_SIZE);
    }

    /* Update break */
    pcb->mm.brk = new_brk;

    return new_brk;
}

virtual_addr_t user_mmap(pcb_t* pcb, virtual_addr_t addr, size_t length,
                        int prot, int flags, int fd, size_t offset) {
    (void)fd;     /* No filesystem yet */
    (void)offset; /* No filesystem yet */
    (void)flags;  /* TODO: Use flags (MAP_PRIVATE, MAP_SHARED, etc.) */

    if (!pcb) {
        return 0;
    }

    /* Round length up to page boundary */
    length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    /* Convert prot to vmap flags */
    uint64_t vmap_flags = VMAP_FLAG_USER;
    if (prot & 0x02) vmap_flags |= VMAP_FLAG_WRITE;  /* PROT_WRITE */
    if (!(prot & 0x04)) vmap_flags |= VMAP_FLAG_NO_EXEC;  /* ~PROT_EXEC */

    /* Find free address if not specified */
    if (addr == 0) {
        addr = USER_BASE + (16 * 1024 * 1024);  /* Start after first 16MB */
        if (pcb->mm.brk > addr) {
            addr = pcb->mm.brk;
        }
        addr = (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }

    /* Allocate and map pages */
    for (size_t i = 0; i < length / PAGE_SIZE; i++) {
        physical_addr_t paddr;
        if (pframe_alloc(&paddr) != PFRAME_OK) {
            /* Rollback */
            user_munmap(pcb, addr, i * PAGE_SIZE);
            return 0;
        }

        vmm_result_t result = vmm_map_to_user(pcb->mm.pml4_phys,
                                               addr + (i * PAGE_SIZE),
                                               paddr, 1,
                                               vmap_flags);
        if (result != VMM_OK) {
            pframe_free(paddr);
            user_munmap(pcb, addr, i * PAGE_SIZE);
            return 0;
        }

        /* Clear the page for anonymous mappings */
        memset((void*)phys_to_virt_offset(paddr), 0, PAGE_SIZE);
    }

    return addr;
}

int user_munmap(pcb_t* pcb, virtual_addr_t addr, size_t length) {
    if (!pcb) {
        return -1;
    }

    /* Validate address is page-aligned */
    if ((addr & (PAGE_SIZE - 1)) != 0) {
        return -1;
    }

    /* Validate address is in user space */
    if (!vmm_is_user_addr(addr)) {
        return -1;
    }

    /* Round length up to page boundary */
    length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    /* Unmap pages */
    for (size_t i = 0; i < length / PAGE_SIZE; i++) {
        virtual_addr_t vaddr = addr + (i * PAGE_SIZE);

        /* Get physical address */
        physical_addr_t paddr;
        if (page_virt_to_phys(pcb->mm.pml4_phys, vaddr, &paddr) == PAGE_OK) {
            pframe_free(paddr);
        }

        /* Unmap the page */
        page_unmap_page(pcb->mm.pml4_phys, vaddr, false);
    }

    return 0;
}

/* ==============================================================================
 * Initialization
 * ============================================================================ */

int user_init(void) {
    klog_info("[USER] Initializing user mode subsystem...\n");
    klog_info("[USER] User stack size: %d MB\n", USER_STACK_SIZE / (1024 * 1024));
    klog_info("[USER] User space: 0x%llX - 0x%llX\n", USER_BASE, USER_END);
    return 0;
}
