# 04 - 实现 COW 核心模块

说实话，当 COW 第一次正常工作时，我盯着屏幕看了好久——这么复杂的东西，居然真的能跑起来。

---

## COW 模块概述

现在我们有了哈希表，可以开始实现 COW 的核心逻辑了。COW 模块的主要职责是：

1. **跟踪 COW 页**：记录哪些物理页是 COW 保护的状态
2. **管理引用计数**：跟踪每个 COW 页有多少个引用
3. **处理页错误**：当写入 COW 页时，执行实际的复制操作
4. **提供 API**：让其他模块可以注册/注销 COW 区域

---

## 创建 kernel/mm/vmm/cow.h

首先创建头文件，定义 COW 模块的接口：

```c
/* ==============================================================================
 * CCOS - Copy-on-Write (COW) Implementation
 * ==============================================================================
 * This module provides copy-on-write memory management, which is essential
 * for efficient fork() system call implementation. Multiple processes can
 * share the same physical pages until one of them attempts to write.
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"
#include "sync/atomic.h"
#include "mm/vmm/page.h"
#include "mm/vmm/vmm_constants.h"
#include "mm/vmm/vmm_config.h"

/* Forward declaration */
struct interrupt_frame;
```

### 数据结构定义

```c
/* ============================================================================
 * COW Data Structures
 * ============================================================================ */

/**
 * @brief COW block - tracks a shared physical page
 */
typedef struct cow_block {
    physical_addr_t    orig_phys;     /* Original physical page address */
    atomic_t           refcount;      /* Atomic reference count (1-COW_MAX_REFCOUNT) */
} cow_block_t;

/**
 * @brief COW region - for tracking larger COW regions (used by fork)
 */
typedef struct cow_region {
    virtual_addr_t     start;         /* Region start (page-aligned) */
    uint64_t           page_count;    /* Number of pages */
    physical_addr_t    pml4;          /* Address space this belongs to */
    struct cow_region* next;          /* Linked list */
} cow_region_t;

/**
 * @brief COW statistics
 */
typedef struct {
    uint64_t    cow_faults_handled;   /* Number of COW faults processed */
    uint64_t    cow_pages_allocated;  /* Total COW pages allocated */
    uint64_t    cow_pages_freed;      /* Total COW pages freed */
    uint64_t    cow_current_blocks;   /* Current number of COW blocks */
    uint64_t    cow_write_faults;     /* Write faults on COW pages */
    uint64_t    cow_coalesced;        /* Pages that didn't need copy (refcount=1) */
} cow_stats_t;

/**
 * @brief COW result codes
 */
typedef enum {
    COW_OK = 0,
    COW_ERR_NOT_INIT = -1,
    COW_ERR_OOM = -2,
    COW_ERR_INVALID = -3,
    COW_ERR_NOT_COW = -4,
    COW_ERR_MAX_REFCOUNT = -5,
} cow_result_t;
```

### 核心 API

```c
/* ============================================================================
 * COW Core API
 * ============================================================================ */

cow_result_t cow_init(void);
bool cow_is_initialized(void);
cow_result_t cow_get_stats(cow_stats_t* stats);

/* ============================================================================
 * COW Page Management
 * ============================================================================ */

cow_result_t cow_add_page(physical_addr_t phys);
cow_result_t cow_inc_refcount(physical_addr_t phys);
cow_result_t cow_dec_refcount(physical_addr_t phys);
cow_result_t cow_get_refcount(physical_addr_t phys, uint16_t* out_refcount);
cow_block_t* cow_lookup_block(physical_addr_t phys);

/* ============================================================================
 * COW Region Management
 * ============================================================================ */

cow_result_t cow_register_region(physical_addr_t pml4,
                                 virtual_addr_t base,
                                 size_t size);
cow_result_t cow_unregister_region(physical_addr_t pml4,
                                   virtual_addr_t base);

/* ============================================================================
 * COW Page Fault Handling
 * ============================================================================ */

cow_result_t cow_handle_fault(physical_addr_t pml4, virtual_addr_t fault_addr);
cow_result_t cow_mark_page_readonly(physical_addr_t pml4, virtual_addr_t vaddr);
cow_result_t cow_make_writable(physical_addr_t pml4,
                               virtual_addr_t vaddr,
                               physical_addr_t phys);
cow_result_t cow_copy_on_write(physical_addr_t pml4,
                               virtual_addr_t vaddr,
                               physical_addr_t orig_phys,
                               cow_block_t* block);
```

---

## 创建 kernel/mm/vmm/cow.c

现在实现这些功能。首先包含头文件和定义内部状态：

```c
/* ==============================================================================
 * CCOS - Copy-on-Write (COW) Implementation
 * ==============================================================================
 */

#include "mm/vmm/cow.h"
#include "mm/vmm/vmm.h"
#include "mm/vmm/page.h"
#include "mm/pframe/pframe.h"
#include "mm/heap/heap.h"
#include "base/hashmap.h"
#include "base/memory.h"
#include "klogs/kprintf.h"
#include "assert/assert.h"
```

### 内部状态

```c
/* ============================================================================
 * Internal State
 * ============================================================================ */

/**
 * @brief Global COW state
 */
static struct {
    hashmap_t*    page_map;       /* cow_block_t* -> cow_block_t */
    cow_region_t* regions;        /* List of COW regions */
    cow_stats_t   stats;
    bool          initialized;
} s_cow_state = {
    .page_map = NULL,
    .regions = NULL,
    .stats = {0},
    .initialized = false
};
```

⚠️ 注意：我们使用指针哈希，把 `cow_block_t*` 作为键。但查找时我们需要通过物理地址查找，所以需要遍历所有条目。

### 哈希函数

```c
/* ============================================================================
 * Hash Function for cow_block_t pointers
 * ============================================================================ */

static size_t hash_cow_block_ptr(const void* key) {
    return hash_ptr(key);
}

static bool eq_cow_block_ptr(const void* a, const void* b) {
    return a == b;
}
```

### 物理地址查找

由于我们使用指针哈希，需要通过物理地址查找时需要遍历：

```c
/**
 * @brief Context for physical address search during iteration
 */
static struct search_ctx {
    physical_addr_t target;
    cow_block_t* result;
} s_search_ctx;

/**
 * @brief Iterator callback for searching by physical address
 */
static bool search_by_phys_iterator(const void* key, void* value, void* context) {
    (void)key;
    struct search_ctx* ctx = (struct search_ctx*)context;
    cow_block_t* block = (cow_block_t*)value;
    if (block->orig_phys == ctx->target) {
        ctx->result = block;
        return false;  /* Stop iteration */
    }
    return true;  /* Continue iteration */
}

/**
 * @brief Look up a COW entry by physical address
 */
static cow_block_t* find_block_by_phys(physical_addr_t phys) {
    if (!s_cow_state.initialized || s_cow_state.page_map == NULL) {
        return NULL;
    }

    phys = phys & ~(PAGE_SIZE - 1);  /* Align to page boundary */

    s_search_ctx.target = phys;
    s_search_ctx.result = NULL;

    hashmap_foreach(s_cow_state.page_map, search_by_phys_iterator, &s_search_ctx);

    return s_search_ctx.result;
}
```

### 初始化

```c
/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

cow_result_t cow_init(void) {
    if (s_cow_state.initialized) {
        klog_warn("[COW] Already initialized\n");
        return COW_OK;
    }

    klog_info("[COW] Initializing Copy-on-Write subsystem...\n");

    /* Create hash map for tracking COW pages */
    s_cow_state.page_map = hashmap_create(COW_HASH_SIZE,
                                         hash_cow_block_ptr,
                                         eq_cow_block_ptr);
    if (s_cow_state.page_map == NULL) {
        klog_error("[COW] Failed to create hash map\n");
        return COW_ERR_OOM;
    }

    /* Initialize stats */
    memset(&s_cow_state.stats, 0, sizeof(cow_stats_t));

    /* Initialize region list */
    s_cow_state.regions = NULL;

    s_cow_state.initialized = true;

    klog_info("[COW] Initialized with %d buckets\n", COW_HASH_SIZE);
    return COW_OK;
}
```

### 页管理 API

```c
/* ============================================================================
 * COW Page Management
 * ============================================================================ */

cow_result_t cow_add_page(physical_addr_t phys) {
    if (!s_cow_state.initialized) {
        return COW_ERR_NOT_INIT;
    }

    /* Align to page boundary */
    phys = phys & ~(PAGE_SIZE - 1);

    /* Check if already tracked */
    cow_block_t* existing = find_block_by_phys(phys);
    if (existing != NULL) {
        /* Already in COW tracking, just increment refcount */
        return cow_inc_refcount(phys);
    }

    /* Create new COW block */
    cow_block_t* block = (cow_block_t*)kmalloc(sizeof(cow_block_t));
    if (block == NULL) {
        return COW_ERR_OOM;
    }

    block->orig_phys = phys;
    atomic_init(&block->refcount, 1);

    /* Add to hash map using block pointer as key */
    int result = hashmap_put(s_cow_state.page_map, block, block);
    if (result != 0) {
        kfree(block);
        return COW_ERR_OOM;
    }

    s_cow_state.stats.cow_pages_allocated++;
    s_cow_state.stats.cow_current_blocks++;

    return COW_OK;
}

cow_result_t cow_inc_refcount(physical_addr_t phys) {
    if (!s_cow_state.initialized) {
        return COW_ERR_NOT_INIT;
    }

    phys = phys & ~(PAGE_SIZE - 1);

    cow_block_t* block = find_block_by_phys(phys);
    if (block == NULL) {
        return COW_ERR_INVALID;
    }

    uint16_t old_refcount = atomic_fetch_add(&block->refcount, 1);
    if (old_refcount >= COW_MAX_REFCOUNT) {
        atomic_fetch_sub(&block->refcount, 1);  /* Rollback */
        return COW_ERR_MAX_REFCOUNT;
    }

    return COW_OK;
}

cow_result_t cow_dec_refcount(physical_addr_t phys) {
    if (!s_cow_state.initialized) {
        return COW_ERR_NOT_INIT;
    }

    phys = phys & ~(PAGE_SIZE - 1);

    cow_block_t* block = find_block_by_phys(phys);
    if (block == NULL) {
        return COW_ERR_INVALID;
    }

    uint16_t old_refcount = atomic_fetch_sub(&block->refcount, 1);

    if (old_refcount == 1) {
        /* Refcount became 0, remove from tracking */
        hashmap_remove(s_cow_state.page_map, block);
        kfree(block);
        s_cow_state.stats.cow_current_blocks--;
        s_cow_state.stats.cow_pages_freed++;
    }

    return COW_OK;
}

cow_block_t* cow_lookup_block(physical_addr_t phys) {
    if (!s_cow_state.initialized) {
        return NULL;
    }

    return find_block_by_phys(phys & ~(PAGE_SIZE - 1));
}
```

⚠️ 注意：`cow_inc_refcount` 中检查 `COW_MAX_REFCOUNT` 是为了防止引用计数溢出。如果 fork() 链太深（超过 65535 次），可能会溢出——这在实践中几乎不可能发生。

### COW 页错误处理

这是 COW 模块的核心功能：

```c
/* ============================================================================
 * COW Page Fault Handling
 * ============================================================================ */

cow_result_t cow_handle_fault(physical_addr_t pml4, virtual_addr_t fault_addr) {
    if (!s_cow_state.initialized) {
        return COW_ERR_NOT_INIT;
    }

    s_cow_state.stats.cow_write_faults++;

    /* 1. Query the current mapping */
    page_query_result_t query;
    if (page_query(pml4, fault_addr, &query) != PAGE_OK) {
        return COW_ERR_INVALID;
    }

    /* 2. Check if it's a COW page */
    if (!cow_is_cow_page(query.flags)) {
        return COW_ERR_NOT_COW;
    }

    /* 3. Look up COW block */
    cow_block_t* block = cow_lookup_block(query.phys_addr);
    if (block == NULL) {
        return COW_ERR_INVALID;
    }

    /* 4. Handle based on refcount */
    uint16_t refcount = atomic_load(&block->refcount);

    if (refcount == 1) {
        /* Only we have this page, just make it writable */
        s_cow_state.stats.cow_coalesced++;
        return cow_make_writable(pml4, fault_addr, query.phys_addr);
    } else {
        /* Multiple references, need to copy */
        return cow_copy_on_write(pml4, fault_addr, query.phys_addr, block);
    }
}

cow_result_t cow_make_writable(physical_addr_t pml4,
                               virtual_addr_t vaddr,
                               physical_addr_t phys) {
    /* Query to get current flags */
    page_query_result_t query;
    if (page_query(pml4, vaddr, &query) != PAGE_OK) {
        return COW_ERR_INVALID;
    }

    /* Clear COW flag and set write permission */
    uint64_t new_flags = cow_clear_cow_flag(query.flags);
    new_flags |= PAGE_FLAG_WRITE;

    /* Update the mapping */
    if (page_update_flags(pml4, vaddr, new_flags) != PAGE_OK) {
        return COW_ERR_INVALID;
    }

    /* Invalidate TLB for this page */
    page_invalidate_tlb(vaddr);

    s_cow_state.stats.cow_faults_handled++;

    return COW_OK;
}

cow_result_t cow_copy_on_write(physical_addr_t pml4,
                               virtual_addr_t vaddr,
                               physical_addr_t orig_phys,
                               cow_block_t* block) {
    /* Allocate new physical page */
    physical_addr_t new_phys;
    if (pframe_alloc(1, &new_phys) != PF_OK) {
        return COW_ERR_OOM;
    }

    /* Map temporarily to copy */
    virtual_addr_t temp_src = 0;
    virtual_addr_t temp_dst = 0;

    /* TODO: Implement temporary mapping and copy */
    /* This is simplified - real implementation needs proper temporary mapping */

    /* Update page table to point to new page */
    uint64_t new_flags = PAGE_FLAG_WRITE | PAGE_FLAG_USER;
    if (page_map_page(pml4, vaddr, new_phys, new_flags, false) != PAGE_OK) {
        pframe_free(new_phys, 1);
        return COW_ERR_INVALID;
    }

    /* Decrease refcount for original page */
    cow_dec_refcount(orig_phys);

    /* Invalidate TLB */
    page_invalidate_tlb(vaddr);

    s_cow_state.stats.cow_faults_handled++;

    return COW_OK;
}
```

⚠️ 注意：上面的 `cow_copy_on_write` 是简化版本。实际实现需要正确处理临时映射和内存复制，这部分代码比较复杂。

---

## 常见陷阱

### 陷阱一：引用计数泄漏

如果进程退出时没有减少引用计数，那些 COW 页永远不会被释放。确保在进程销毁时调用 `cow_unregister_region`。

### 陷阱二：页表同步问题

多个虚拟地址可能映射到同一个物理页。COW 只处理触发页错误的那个虚拟地址，其他映射仍然指向原页——这是正确的行为。

### 陷阱三：递归页错误

COW 处理过程中如果再次触发页错误（比如 kmalloc 导致页错误），会导致递归。解决方案是预分配内存或使用专门的内存池。

---

## 下一步

现在我们已经实现了 COW 核心模块，但它还不能工作——我们需要在页错误处理器中集成 COW 处理逻辑。

下一节我们将实现 x86_64 异常处理机制的基础知识，包括什么是异常、异常分类、错误码格式等。这些知识是实现异常处理器的基础。

在继续之前，请确保你理解了 COW 模块的 API 和工作原理。后续的代码会直接使用这些接口。
