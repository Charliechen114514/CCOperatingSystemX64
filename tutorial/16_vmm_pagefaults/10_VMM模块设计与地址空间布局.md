# 10 - VMM 模块设计与地址空间布局

说实话，设计地址空间布局是内核开发中最容易后悔的决策之一。一旦你决定了某个区域放在哪里，以后想改就很难了。

---

## x86_64 的规范地址要求

在讨论具体布局之前，我们需要理解 x86_64 的规范地址（Canonical Address）概念。x86_64 要求虚拟地址的位 63-48 必须是位 47 的符号扩展。换句话说，如果位 47 是 0，那么位 63-48 也必须全是 0；如果位 47 是 1，那么位 63-48 也必须全是 1。

满足这个条件的地址叫做规范地址，不满足的地址叫做非规范地址，访问它们会触发通用保护异常（#GP）。

基于这个要求，x86_64 的地址空间被划分为用户空间（低半部分）和内核空间（高半部分）。用户空间从 `0x0000000000000000` 到 `0x00007FFFFFFFFFFF`（128TB），实际从 0x400000 开始，跳过 NULL 页。内核空间从 `0xFFFF800000000000` 到 `0xFFFFFFFFFFFFFFFF`（128TB）。两者之间是非规范空洞，访问无效。

虽然理论上内核空间有 128TB，但实际能用的地址受限于虚拟地址的位数。当前 x86_64 实现只使用 48 位虚拟地址，所以内核空间的实际范围是 `0xFFFF800000000000 - 0xFFFFFFFFFFFFFFFF`。

---

## 我们的内核地址空间布局

内核地址空间的布局是经过深思熟虑的。直接映射区放在内核空间的起始位置，这使得物理地址到虚拟地址的转换非常简单：`virt = phys + 0xFFFF800000000000`。内核代码区放在高地址，这遵循了"高半内核"（Higher-Half Kernel）的设计模式，让内核代码和用户代码完全分离。内核堆区独立于数据区，这为未来的 kmalloc 实现预留了空间。

具体来说，我们的布局是这样的：

```
直接映射区:    0xFFFF800000000000 - 0xFFFF800001000000 (256MB)
内核代码区:    0xFFFFFFFF80000000 - 0xFFFFFFFF80200000 (2MB)
内核数据区:    0xFFFFFFFF80200000 - 0xFFFFFFFF80400000 (2MB)
内核堆区:      0xFFFFFFFF81000000 - 0xFFFFFFFF89000000 (128MB)
```

---

## vmm.h 中的常量定义

在 `vmm.h` 中，我们定义了这些地址边界。首先是直接物理映射区域，然后是内核代码和数据区域，接着是内核堆区域（为未来的 kmalloc 实现预留），最后是物理映射偏移量和用户空间边界。

```c
/* Direct physical map region */
#define KERNEL_VIRT_BASE        0xFFFF800000000000ULL
#define KERNEL_VIRT_END         0xFFFF800001000000ULL  /* 256MB direct map */

/* Kernel code/data regions */
#define KERNEL_TEXT_BASE        0xFFFFFFFF80000000ULL
#define KERNEL_TEXT_SIZE        (2 * 1024 * 1024)     /* 2MB */
#define KERNEL_DATA_BASE        (KERNEL_TEXT_BASE + KERNEL_TEXT_SIZE)
#define KERNEL_DATA_SIZE        (2 * 1024 * 1024)     /* 2MB */

/* Kernel heap region (for future kmalloc implementation) */
#define KERNEL_HEAP_BASE        0xFFFFFFFF81000000ULL
#define KERNEL_HEAP_SIZE        (128 * 1024 * 1024)   /* 128MB */
#define KERNEL_HEAP_MAX         (KERNEL_HEAP_BASE + KERNEL_HEAP_SIZE)

/* Physical map offset for direct mapping */
#define PHYS_MAP_OFFSET         KERNEL_VIRT_BASE

/* User space boundaries */
#define USER_BASE               0x0000000000400000ULL  /* 4MB (skip NULL page) */
#define USER_END                0x00007FFFFFFFFFFFULL  /* 128TB user limit */
```

---

## 地址转换与检查宏

为了方便地在物理地址和虚拟地址之间转换，我们定义了两个宏。这些宏非常简单：把物理地址加上偏移量得到虚拟地址，或者把虚拟地址减去偏移量得到物理地址。

```c
static inline virtual_addr_t phys_to_virt_offset(physical_addr_t phys) {
    return phys + PHYS_MAP_OFFSET;
}

static inline physical_addr_t virt_to_phys_offset(virtual_addr_t virt) {
    return virt - PHYS_MAP_OFFSET;
}
```

这些宏只适用于直接映射区域的虚拟地址。如果你传入一个不在直接映射区域的虚拟地址，转换结果是错误的。

为了判断一个地址属于哪个区域，我们定义了一些检查宏：

```c
static inline bool vmm_is_kernel_addr(virtual_addr_t addr) {
    return addr >= KERNEL_VIRT_BASE;
}

static inline bool vmm_is_user_addr(virtual_addr_t addr) {
    return addr >= USER_BASE && addr < USER_END;
}

static inline bool vmm_is_canonical(virtual_addr_t addr) {
    return ((addr >> 48) == 0) || ((addr >> 48) == 0xFFFF);
}
```

这些宏在页错误处理和权限检查时非常有用。

---

## 跟踪已映射的内存区域

为了跟踪已映射的内存区域，我们定义了 `memory_region_t` 结构。这个结构存储一个内存区域的起始地址、结束地址、对应的物理地址、权限标志和名称。

```c
typedef struct memory_region {
    virtual_addr_t start;       /* Region start (inclusive) */
    virtual_addr_t end;         /* Region end (exclusive) */
    physical_addr_t phys_start; /* Physical address (0 if unmapped) */
    uint64_t flags;             /* Protection flags */
    char name[32];              /* Region name for debugging */
} memory_region_t;
```

跟踪已映射的区域有几个用途。首先可以防止重复映射，在映射新内存时，检查是否与已有区域冲突。其次可以提供调试信息，打印当前地址空间的布局。最后可以进行权限检查，快速判断一个地址是否属于某个特定区域。

---

## VMM 统计信息

为了监控虚拟内存的使用情况，我们定义了统计信息结构。这些统计信息可以帮助我们了解虚拟内存的使用情况，比如有多少虚拟页面被映射、内核空间和用户空间各占多少、分配了多少页表。

```c
typedef struct {
    uint64_t total_pages;       /* Total virtual pages managed */
    uint64_t mapped_pages;      /* Currently mapped pages */
    uint64_t kernel_pages;      /* Kernel space pages */
    uint64_t user_pages;        /* User space pages */
    uint64_t page_tables;       /* Number of page tables allocated */
} vmm_stats_t;
```

---

## VMM 错误码定义

和 `page.h` 类似，我们定义了一组错误码。这些错误码会被 VMM 的所有函数使用，调用者可以通过检查返回值来判断操作是否成功。

```c
typedef enum {
    VMM_OK = 0,
    VMM_ERR_NOT_INIT = -1,
    VMM_ERR_OOM = -2,
    VMM_ERR_INVALID = -3,
    VMM_ERR_PERM = -4,          /* Permission denied */
    VMM_ERR_NOT_MAPPED = -5,
    VMM_ERR_ALREADY_MAPPED = -6,
} vmm_result_t;
```

---

## 下一步

现在我们已经设计好了 VMM 模块的地址空间布局和数据结构。接下来我们需要实现虚拟页面分配器 `vmm_alloc_pages`，它可以根据请求的大小分配虚拟页面。在下一个文档中，我们会详细讲解 `vmm_alloc_pages` 和 `vmm_free_pages` 的实现，以及虚拟地址分配策略。
