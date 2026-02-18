# 06 - 实现核心映射函数 page_map_page

说实话，`page_map_page` 是我写过的最复杂的函数之一。它不仅要处理四级页表的遍历，还要处理巨大页、自动分配中间页表、错误检查……每一步都可能出错。

---

## 函数签名设计

让我们先看看这个函数的签名：

```c
page_result_t page_map_page(physical_addr_t pml4_phys,
                           virtual_addr_t vaddr,
                           physical_addr_t paddr,
                           uint64_t flags,
                           bool alloc_missing);
```

### 为什么需要 pml4_phys 参数

你可能会问：为什么要传入 PML4 物理地址？我们不是已经在 `page_init` 中保存了内核 PML4 地址吗？

答案是：我们需要支持用户地址空间。每个进程都有自己的 PML4，当我们为用户空间映射内存时，需要使用用户的 PML4，而不是内核的 PML4。

通过传入 `pml4_phys` 参数，这个函数可以同时支持内核地址空间和用户地址空间的映射。

### alloc_missing 参数的作用

`alloc_missing` 参数控制是否自动分配缺失的中间页表。

当 `alloc_missing = true` 时，如果遍历过程中发现某个中间页表不存在，函数会自动分配一个新的页表。当 `alloc_missing = false` 时，如果中间页表不存在，函数会返回错误。

这个参数在很多场景下非常有用。比如，当你只想查询一个映射是否存在时，可以设置 `alloc_missing = false`，避免意外创建新的页表。

---

## 标志位转换

我们的 API 使用 `VMAP_FLAG_*` 格式的标志位，但页表项需要的是硬件格式的标志位。我们需要一个转换函数：

```c
static inline uint64_t vmap_flags_to_pte(uint64_t vmap_flags) {
    uint64_t pte_flags = PAGE_PRESENT;

    if (vmap_flags & VMAP_FLAG_WRITE) {
        pte_flags |= PAGE_WRITE;
    }
    if (vmap_flags & VMAP_FLAG_USER) {
        pte_flags |= PAGE_USER;
    }
    if (vmap_flags & VMAP_FLAG_NO_EXEC) {
        pte_flags |= PAGE_NO_EXEC;
    }
    if (vmap_flags & VMAP_FLAG_WRITE_THRU) {
        pte_flags |= PAGE_WRITE_THRU;
    }
    if (vmap_flags & VMAP_FLAG_NO_CACHE) {
        pte_flags |= PAGE_NO_CACHE;
    }

    return pte_flags;
}
```

这个函数把我们的软件标志转换成硬件标志。注意 `PAGE_PRESENT` 总是被设置，因为映射一个页面意味着它必须在内存中。

---

## 参数验证

函数的第一步是验证参数的有效性：

```c
/* Determine page size from flags */
bool use_2mb = (flags & VMAP_FLAG_HUGE_2MB) != 0;
bool use_1gb = (flags & VMAP_FLAG_HUGE_1GB) != 0;

/* Validate only one huge page flag is set */
if (use_2mb && use_1gb) {
    klog_error("[PAGE] Cannot set both HUG_2MB and HUG_1GB flags\n");
    return PAGE_ERR_INVALID;
}

/* Determine alignment requirements */
uint64_t page_size = PAGE_SIZE;
uint64_t page_align = PAGE_SIZE - 1;

if (use_1gb) {
    page_size = PAGE_SIZE_1GB;
    page_align = page_size - 1;
} else if (use_2mb) {
    page_size = PAGE_SIZE_2MB;
    page_align = page_size - 1;
}

/* Validate alignment */
if ((vaddr & page_align) != 0) {
    klog_error("[PAGE] Virtual address not aligned to %llu bytes: 0x%llX\n",
              page_size, vaddr);
    return PAGE_ERR_ALIGNMENT;
}
if ((paddr & page_align) != 0) {
    klog_error("[PAGE] Physical address not aligned to %llu bytes: 0x%X\n",
              page_size, paddr);
    return PAGE_ERR_ALIGNMENT;
}
```

### 对齐检查的原因

硬件要求页表的每个级别都必须正确对齐：

- 4KB 页面必须 4KB 对齐
- 2MB 巨大页必须 2MB 对齐
- 1GB 巨大页必须 1GB 对齐

如果你传入一个不对齐的地址，硬件会忽略低位的地址位，导致映射到错误的物理地址。这种 bug 非常难以调试，因为症状看起来像是"随机"的。

⚠️ 注意：对齐检查使用 `& (page_size - 1)` 而不是取模运算。这是因为 `page_size` 总是 2 的幂次方，位运算比除法快得多。

---

## 四级页表遍历

接下来是函数的核心部分：遍历四级页表并创建映射。

### 第一步：获取 PML4

```c
/* Get PML4 */
pml4_t* pml4 = (pml4_t*)phys_to_virt(pml4_phys);
page_table_entry_t* pml4e = &pml4->entries[PML4_INDEX(vaddr)];
```

我们使用 `phys_to_virt` 把物理地址转换成虚拟地址指针，然后通过 `PML4_INDEX(vaddr)` 宏获取对应的 PML4 项。

⚠️ 注意：这里假设直接映射已经建立。如果 `s_direct_map_established` 还是 `false`，使用 `phys_to_virt` 会导致访问无效内存。

### 第二步：获取或创建 PDPT

```c
/* Walk or create PDPT */
physical_addr_t pdpt_phys = pml4e->bits.frame << PAGE_SHIFT;
page_result_t result = get_or_create_table(&pdpt_phys, alloc_missing);
if (result != PAGE_OK) {
    return result;
}

/* Update PML4 entry if we created a new PDPT */
if (pdpt_phys != (pml4e->bits.frame << PAGE_SHIFT)) {
    pml4e->value = (pdpt_phys & PTE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
}
```

我们首先尝试从 PML4 项中读取 PDPT 的物理地址。如果 PML4 项的 Present 位为 0，`pdpt_phys` 会是 0。

`get_or_create_table` 函数会检查 `pdpt_phys` 是否为 0：

```c
static page_result_t get_or_create_table(physical_addr_t* table_phys,
                                         bool alloc_missing) {
    if (*table_phys != 0) {
        return PAGE_OK;
    }

    if (!alloc_missing) {
        return PAGE_ERR_NOT_PRESENT;
    }

    /* Allocate new table */
    return page_create_table(table_phys);
}
```

如果 `pdpt_phys` 不为 0，说明 PDPT 已经存在，直接返回 `PAGE_OK`。如果 `pdpt_phys` 为 0 且 `alloc_missing` 为 `true`，分配一个新的页表。

如果我们创建了新的 PDPT，需要更新 PML4 项，使它指向新的 PDPT。

⚠️ 注意：更新 PML4 项时使用的标志位是 `PAGE_PRESENT | PAGE_WRITE | PAGE_USER`。这意味着这个 PDPT 可以被用户访问。这是为了让用户地址空间能够正常工作。

### 第三步：获取或创建 PD

```c
/* Get PDPT */
pdpt_t* pdpt = (pdpt_t*)phys_to_virt(pdpt_phys);
page_table_entry_t* pdpte = &pdpt->entries[PDPT_INDEX(vaddr)];

/* Handle 1GB huge page mapping */
if (use_1gb) {
    if (pdpte->bits.present) {
        klog_error("[PAGE] PDPT entry already present at 0x%llX\n", vaddr);
        return PAGE_ERR_ALREADY_MAPPED;
    }

    /* Create 1GB huge page mapping at PDPT level */
    pdpte->value = (paddr & PTE_ADDR_MASK) | pte_flags | PAGE_HUGE_PDPT;

    /* Invalidate TLB */
    page_invalidate_tlb(vaddr);

    return PAGE_OK;
}

/* Walk or create PD */
physical_addr_t pd_phys = pdpte->bits.frame << PAGE_SHIFT;
result = get_or_create_table(&pd_phys, alloc_missing);
if (result != PAGE_OK) {
    return result;
}

/* Update PDPT entry if we created a new PD */
if (pd_phys != (pdpte->bits.frame << PAGE_SHIFT)) {
    pdpte->value = (pd_phys & PTE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
}
```

这里我们处理 1GB 巨大页的特殊情况。如果请求的是 1GB 巨大页，我们在 PDPT 级别就停止遍历，直接在 PDPT 项中创建映射。

⚠️ 注意：1GB 巨大页使用 `PAGE_HUGE_PDPT` 标志（位 7），这告诉 CPU 这是一个 1GB 页，不需要继续查找 PD 和 PT。

### 第四步：获取或创建 PT（或创建 2MB 巨大页）

```c
/* Get PD */
pd_t* pd = (pd_t*)phys_to_virt(pd_phys);
page_table_entry_t* pde = &pd->entries[PD_INDEX(vaddr)];

/* Handle 2MB huge page mapping */
if (use_2mb) {
    if (pde->bits.present) {
        klog_error("[PAGE] PD entry already present at 0x%llX\n", vaddr);
        return PAGE_ERR_ALREADY_MAPPED;
    }

    /* Create 2MB huge page mapping at PD level */
    pde->value = (paddr & PTE_ADDR_MASK) | pte_flags | PAGE_HUGE_PD;

    /* Invalidate TLB */
    page_invalidate_tlb(vaddr);

    return PAGE_OK;
}

/* Check if PD entry is a huge page */
if (pde->bits.present && pde->bits.pat) {
    /* PD entry is a 2MB huge page. We need to break it into 4KB pages. */
    /* For now, return an error as this is a complex operation. */
    klog_error("[PAGE] Cannot map 4KB page in 2MB huge page region at 0x%llX\n", vaddr);
    return PAGE_ERR_INVALID;
}

/* Walk or create PT */
physical_addr_t pt_phys = pde->bits.frame << PAGE_SHIFT;
result = get_or_create_table(&pt_phys, alloc_missing);
if (result != PAGE_OK) {
    return result;
}

/* Update PD entry if we created a new PT */
if (pt_phys != (pde->bits.frame << PAGE_SHIFT)) {
    pde->value = (pt_phys & PTE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
}
```

这里我们处理 2MB 巨大页的情况。和 1GB 巨大页类似，我们在 PD 级别停止遍历，直接创建映射。

⚠️ 注意：在创建 4KB 页面之前，我们检查 PD 项是否已经是一个 2MB 巨大页。如果是，我们返回错误。这是因为把一个 2MB 巨大页拆分成 512 个 4KB 页面是一个复杂的操作，当前的实现不支持。

### 第五步：创建最终的映射

```c
/* Get PT and set PTE */
pt_t* pt = (pt_t*)phys_to_virt(pt_phys);
page_table_entry_t* pte = &pt->entries[PT_INDEX(vaddr)];

/* Check if already mapped */
if (pte->bits.present) {
    klog_warn("[PAGE] Page already mapped at 0x%llX -> 0x%X\n",
              vaddr, pte->bits.frame << PAGE_SHIFT);
    return PAGE_ERR_ALREADY_MAPPED;
}

/* Set the 4KB page PTE */
pte->value = (paddr & PTE_ADDR_MASK) | pte_flags;

/* Invalidate TLB for this address */
page_invalidate_tlb(vaddr);

return PAGE_OK;
```

最后，我们到达 PT 级别，创建最终的 4KB 页面映射。如果 PT 项已经存在（Present 位为 1），我们返回错误，避免覆盖已有的映射。

⚠️ 注意：每次修改页表后，我们都调用 `page_invalidate_tlb` 来刷新 TLB。这是必须的，否则 CPU 可能继续使用旧的映射结果。

---

## 递归映射的风险

在实现页表管理时，一个常见的陷阱是递归映射（Recursive Mapping）。如果你不小心把一个页表映射到自己，或者创建了循环引用，CPU 在遍历页表时会陷入无限循环。

⚠️ 注意：我们的实现通过以下方式避免递归映射：

1. 每个页表都使用独立的物理帧，不会复用
2. 页表项指向的物理地址总是小于 `KERNEL_VIRT_BASE`（物理地址）
3. 我们从不把页表映射到页表本身的虚拟地址范围

如果你要修改这个实现，务必小心不要引入递归映射。

---

## 标志位的正确传递

另一个常见的陷阱是标志位没有正确传递。比如，你创建了一个用户映射，但中间页表的标志位没有设置 `PAGE_USER`，这样用户程序实际上无法访问这个映射。

⚠️ 注意：在我们的实现中，我们总是为中间页表设置 `PAGE_PRESENT | PAGE_WRITE | PAGE_USER`。这样确保映射可以被正确访问。

---

## 第一阶段测试

现在我们可以测试一下 `page_map_page` 是否正常工作：

```c
/* Test: Map a page at virtual address 0xFFFFFFFF81000000 */
virtual_addr_t test_vaddr = 0xFFFFFFFF81000000;
physical_addr_t test_paddr;
pframe_alloc(&test_paddr);

page_result_t result = page_map_page(
    page_get_pml4(),
    test_vaddr,
    test_paddr,
    VMAP_FLAG_WRITE,
    true
);

if (result == PAGE_OK) {
    klog_info("[TEST] Successfully mapped 0x%llX -> 0x%X\n", test_vaddr, test_paddr);

    /* Try to write to the mapped page */
    volatile uint64_t* ptr = (volatile uint64_t*)test_vaddr;
    *ptr = 0xDEADBEEF;
    klog_info("[TEST] Write successful, value = 0x%llX\n", *ptr);
} else {
    klog_error("[TEST] Failed to map page: %d\n", result);
}
```

编译并运行，你应该能看到类似这样的输出：

```
[TEST] Successfully mapped 0xFFFFFFFF81000000 -> 0x100000
[TEST] Write successful, value = 0xDEADBEEF
```

---

## 常见问题

### 问题 1：映射成功，但读取时崩溃

这通常是因为 TLB 没有刷新。确保在修改页表后调用了 `page_invalidate_tlb`。

### 问题 2：映射成功，但写入时崩溃

这可能是标志位设置错误。检查是否设置了 `PAGE_WRITE`，以及中间页表的标志位是否正确。

### 问题 3：总是返回 PAGE_ERR_ALIGNMENT

这通常意味着地址没有正确对齐。确保虚拟地址和物理地址都按照页面大小对齐（4KB、2MB 或 1GB）。

---

## 下一步

现在我们已经实现了核心的映射函数，接下来我们需要实现查询函数 `page_query` 和地址转换函数 `page_virt_to_phys`。

在下一个文档中，我们会详细讲解这些函数的实现，以及 TLB 刷新的时机和方法。
