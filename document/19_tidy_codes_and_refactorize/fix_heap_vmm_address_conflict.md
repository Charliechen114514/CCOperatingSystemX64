# Fix Heap and VMM Address Space Conflict

## Problem Description

When running VMM demo followed by Heap demo, the heap expansion failed with:
```
[ERROR] [PAGE] Page already mapped at 0xFFFFFFFF81015000 -> 0x246000
[ERROR] [HEAP] Failed to allocate pages at 0xFFFFFFFF81015000 for expansion
[ERROR] [HEAP] Out of memory
```

### Root Cause

1. **VMM Demo** uses `vmm_alloc_pages()` to allocate pages, which searches for free virtual addresses starting from `KERNEL_HEAP_BASE` (0xFFFFFFFF81000000)

2. **Heap Allocator** uses `vmm_alloc_pages_at()` at a fixed address (`heap_brk`) to maintain contiguous heap memory

3. **Conflict**: Both systems tried to use the same virtual address range (KERNEL_HEAP_BASE), causing:
   - VMM Demo allocated pages at 0xFFFFFFFF81015000
   - Heap later tried to expand at 0xFFFFFFFF81015000 (same address)
   - Page allocation failed due to "already mapped" error

### Initial Memory Layout (Before Fix)

```
0xFFFFFFFF81000000  <- KERNEL_HEAP_BASE
    ↓
    [VMM Demo Allocation]  <- vmm_alloc_pages() starts here
    [Heap Allocation]      <- heap tries to use fixed addresses
    ↓
0xFFFFFFFF810800000  <- KERNEL_HEAP_MAX (128MB)
```

## Solution

Separate the virtual address spaces into two distinct regions:

### 1. Add General Allocation Region

**File: `kernel/mm/vmm/vmm_config.h`**
```c
/* Kernel heap region - exclusively for heap allocator */
#define KERNEL_HEAP_BASE        0xFFFFFFFF81000000ULL
#define KERNEL_HEAP_SIZE        (128 * 1024 * 1024)   /* 128MB */
#define KERNEL_HEAP_MAX         (KERNEL_HEAP_BASE + KERNEL_HEAP_SIZE)

/* General kernel allocation region (for VMM demo, temporary mappings, etc.) */
#define KERNEL_GENERAL_BASE     0xFFFFFFFF88000000ULL
#define KERNEL_GENERAL_SIZE     (128 * 1024 * 1024)   /* 128MB */
#define KERNEL_GENERAL_MAX      (KERNEL_GENERAL_BASE + KERNEL_GENERAL_SIZE)
```

### 2. Update VMM Initialization

**File: `kernel/mm/vmm/vmm.c`**

Changed initial hint and registered heap region as reserved:
```c
/* Virtual address allocation hint for kernel mappings */
static virtual_addr_t s_kernel_virt_hint = KERNEL_GENERAL_BASE;

static vmm_result_t find_free_virt_range(...) {
    virtual_addr_t hint = s_kernel_virt_hint;
    virtual_addr_t end = KERNEL_GENERAL_MAX;  /* Search within general region */
    ...
}
```

In `vmm_init()`:
```c
/* Register heap region as reserved so vmm_alloc_pages doesn't use it */
add_region(KERNEL_HEAP_BASE, KERNEL_HEAP_MAX, 0, VMAP_FLAG_NONE, "kernel_heap");
```

### New Memory Layout (After Fix)

```
0xFFFFFFFF81000000  <- KERNEL_HEAP_BASE
    ↓
    [Heap Allocation Only]  <- heap uses fixed addresses here
    ↓
0xFFFFFFFF810800000  <- KERNEL_HEAP_MAX

0xFFFFFFFF88000000  <- KERNEL_GENERAL_BASE
    ↓
    [VMM Demo]            <- vmm_alloc_pages() uses this region
    [Temporary Mappings]
    ↓
0xFFFFFFFF880800000  <- KERNEL_GENERAL_MAX
```

## Key Changes

1. **`vmm_config.h`**: Added `KERNEL_GENERAL_BASE` and `KERNEL_GENERAL_MAX` defines
2. **`vmm.c`**:
   - Changed `s_kernel_virt_hint` from `KERNEL_HEAP_BASE` to `KERNEL_GENERAL_BASE`
   - Changed `find_free_virt_range()` to use `KERNEL_GENERAL_MAX` as search limit
   - Registered heap region as reserved in `vmm_init()`
3. **`heap.c`**: No changes needed (continues using fixed address allocation)

## Benefits

1. **Clear separation**: Heap and general allocations use distinct address ranges
2. **No conflicts**: VMM demo allocations won't interfere with heap
3. **Heap contiguity**: Heap maintains contiguous memory using fixed addresses
4. **Scalability**: Both regions have 128MB of space

## Testing

After the fix:
- VMM Demo successfully allocates pages at 0xFFFFFFFF88000000+
- Heap successfully expands at 0xFFFFFFFF81000000+
- Both demos run without address conflicts
