# 10 - 内核集成与 COW 演示程序

说实话，当内核成功启动并且 COW 演示程序正常运行时，我长舒了一口气——这么多复杂的组件，终于能一起工作了。

---

## 内核集成概述

现在我们已经实现了所有组件，最后一步是把它们集成到内核中：

1. **修改 kernel_init.c**：初始化顺序很重要
2. **创建 COW 演示程序**：验证 COW 功能
3. **编译验证**：确保一切正常

---

## 修改 kernel/kernel_init.c

内核初始化的顺序非常关键。某些模块必须在其他模块之前初始化：

```c
/* In kernel_init.c */

#include "mm/vmm/cow.h"
#include "interrupt/exception.h"
#include "interrupt/gdt.h"
#include "interrupt/tss.h"
#include "demo/cow/cow_demo.h"

void kernel_init(void) {
    /* 1. 基础系统初始化 */
    klog_init();
    heap_init();           /* 堆内存必须在其他模块之前初始化 */

    /* 2. VMM 和页表初始化 */
    vmm_init();
    pframe_init();

    /* 3. 中断系统初始化 */
    idt_init();

    /* 4. GDT 和 TSS 初始化（必须在异常处理之前） */
    tss_init();            /* TSS 先初始化 */
    gdt_init();            /* GDT 会加载 TSS */

    /* 5. COW 模块初始化 */
    cow_init();

    /* 6. 异常处理器初始化 */
    exception_init();      /* 注册 #DF, #SS, #GP 处理器 */
    pf_init();             /* 注册 #PF 处理器（带 COW 支持） */

    /* 7. 其他初始化 */
    /* ... */

    /* 8. 运行演示程序 */
    klog_info("\n");
    klog_info("=================================================================\n");
    klog_info("Running COW demonstration...\n");
    klog_info("=================================================================\n");
    cow_demo_run();

    klog_info("\n");
    klog_info("=================================================================\n");
    klog_info("CCOS Stage 18 initialization complete!\n");
    klog_info("=================================================================\n");
}
```

⚠️ 注意：初始化顺序非常关键。TSS 必须在 GDT 之前初始化，因为 GDT 需要引用 TSS。异常处理器必须在 COW 初始化之后注册，因为 #PF 处理器会调用 COW 函数。

---

## 创建 COW 演示程序

### 创建 kernel/demo/cow/cow_demo.h

```c
/* ==============================================================================
 * CCOS - Copy-on-Write Demo
 * ==============================================================================
 */

#pragma once

#include "defines/types.h"

/**
 * @brief Run the COW demonstration
 */
void cow_demo_run(void);
```

### 创建 kernel/demo/cow/cow_demo.c

```c
/* ==============================================================================
 * CCOS - Copy-on-Write Demo Implementation
 * ==============================================================================
 */

#include "demo/cow/cow_demo.h"
#include "mm/vmm/cow.h"
#include "mm/vmm/vmm.h"
#include "mm/vmm/page.h"
#include "mm/pframe/pframe.h"
#include "mm/heap/heap.h"
#include "klogs/kprintf.h"

/* ============================================================================
 * Demo Helper Functions
 * ============================================================================ */

/**
 * @brief Test 1: Basic COW functionality
 */
static void test_cow_basic(void) {
    klog_info("\n[Test 1] Basic COW functionality\n");
    klog_info("----------------------------------------\n");

    /* Allocate two virtual pages */
    virtual_addr_t page1 = vmm_alloc_pages(1, VMAP_FLAG_WRITE);
    virtual_addr_t page2 = vmm_alloc_pages(1, VMAP_FLAG_WRITE);

    if (page1 == 0 || page2 == 0) {
        klog_error("Failed to allocate virtual pages\n");
        return;
    }

    /* Write some data to page1 */
    volatile uint32_t* p1 = (volatile uint32_t*)page1;
    p1[0] = 0xDEADBEEF;
    p1[1] = 0x12345678;

    klog_info("Written to page1: 0x%X, 0x%X\n", p1[0], p1[1]);

    /* Get physical address of page1 */
    page_query_result_t query;
    physical_addr_t pml4 = vmm_get_current_pml4();
    page_query(pml4, page1, &query);
    physical_addr_t phys1 = query.phys_addr;

    klog_info("page1 physical: 0x%llX\n", phys1);

    /* Unmap page2 and remap to same physical page */
    page_unmap_page(pml4, page2, false);
    page_map_page(pml4, page2, phys1, VMAP_FLAG_WRITE, false);

    /* Add to COW tracking with refcount = 2 */
    cow_add_page(phys1);
    cow_inc_refcount(phys1);

    /* Mark both pages as read-only COW */
    cow_mark_page_readonly(pml4, page1);
    cow_mark_page_readonly(pml4, page2);

    klog_info("Both pages marked as COW\n");

    /* Now try to write to page1 - should trigger COW */
    klog_info("Attempting COW write to page1...\n");
    p1[2] = 0xCAFEBABE;

    klog_info("COW write successful! page1[2] = 0x%X\n", p1[2]);

    /* Verify page2 still has original data */
    volatile uint32_t* p2 = (volatile uint32_t*)page2;
    klog_info("page2 still has: 0x%X, 0x%X (should be unchanged)\n", p2[0], p2[1]);

    /* Print COW statistics */
    cow_stats_t stats;
    cow_get_stats(&stats);
    klog_info("\nCOW Statistics:\n");
    klog_info("  Write faults: %llu\n", stats.cow_write_faults);
    klog_info("  Pages allocated: %llu\n", stats.cow_pages_allocated);
    klog_info("  Current blocks: %llu\n", stats.cow_current_blocks);

    /* Cleanup */
    cow_dec_refcount(phys1);  /* Should remove from tracking */
    vmm_free_pages(page1, 1);
    vmm_free_pages(page2, 1);
}

/**
 * @brief Test 2: COW region registration
 */
static void test_cow_region(void) {
    klog_info("\n[Test 2] COW region registration\n");
    klog_info("----------------------------------------\n");

    /* Allocate a 4-page region */
    virtual_addr_t region = vmm_alloc_pages(4, VMAP_FLAG_WRITE);
    if (region == 0) {
        klog_error("Failed to allocate region\n");
        return;
    }

    /* Write some data */
    volatile uint32_t* ptr = (volatile uint32_t*)region;
    for (int i = 0; i < 16; i++) {
        ptr[i] = 0x1000 + i;
    }

    klog_info("Allocated 4-page region at 0x%llX\n", region);

    /* Register as COW region */
    physical_addr_t pml4 = vmm_get_current_pml4();
    cow_result_t result = cow_register_region(pml4, region, 4 * PAGE_SIZE);

    if (result == COW_OK) {
        klog_info("Region registered as COW\n");
    } else {
        klog_error("Failed to register COW region: %d\n", result);
        vmm_free_pages(region, 4);
        return;
    }

    /* Try to write - should trigger COW */
    klog_info("Attempting COW write to region...\n");
    ptr[0] = 0xBEEFBEEF;

    klog_info("COW write successful!\n");

    /* Print statistics */
    cow_stats_t stats;
    cow_get_stats(&stats);
    klog_info("Current COW blocks: %llu\n", stats.cow_current_blocks);

    /* Cleanup */
    cow_unregister_region(pml4, region);
    vmm_free_pages(region, 4);
}

/* ============================================================================
 * Demo Entry Point
 * ============================================================================ */

void cow_demo_run(void) {
    klog_info("\n");
    klog_info("=================================================================\n");
    klog_info("COW Demonstration Program\n");
    klog_info("=================================================================\n");

    test_cow_basic();
    test_cow_region();

    cow_stats_t stats;
    cow_get_stats(&stats);

    klog_info("\n");
    klog_info("=================================================================\n");
    klog_info("Final COW Statistics:\n");
    klog_info("  Total write faults:     %llu\n", stats.cow_write_faults);
    klog_info("  Total pages allocated:  %llu\n", stats.cow_pages_allocated);
    klog_info("  Current COW blocks:     %llu\n", stats.cow_current_blocks);
    klog_info("  Pages coalesced:        %llu\n", stats.cow_coalesced);
    klog_info("=================================================================\n");

    klog_info("\n[COW Demo] All tests completed successfully!\n");
}
```

---

## 更新 CMakeLists.txt

把新文件加入构建系统：

```cmake
# kernel/demo/CMakeLists.txt

add_subdirectory(cow)

# kernel/demo/cow/CMakeLists.txt

target_sources(cow_demo
    PRIVATE
        cow_demo.h
        cow_demo.c
)

target_link_libraries(cow_demo
    PRIVATE
        kernel_vmm
        kernel_interrupt
        kernel_base
)
```

---

## 编译验证

现在让我们编译并运行内核：

```bash
# 配置构建
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 构建
cmake --build build

# 运行
qemu-system-x86_64 -drive format=raw,file=build/boot.img -serial stdio
```

### 预期输出

你应该能看到类似以下的输出：

```
[HEAP] Kernel heap initialized
[VMM] Virtual memory manager initialized
[PAGE] Page tables initialized
[TSS] TSS initialized
[TSS]   IST1 (DF): 0xXXXXX
[TSS]   IST4 (SS): 0xXXXXX
[GDT] Setting up kernel GDT...
[GDT] GDT loaded at 0xXXXXX
[COW] Initializing Copy-on-Write subsystem...
[COW] Initialized with 64 buckets
[EXC] Registering exception handlers...
[EXC] Exception handlers registered
[PF] Page fault handler registered

=================================================================
Running COW demonstration...
=================================================================

[Test 1] Basic COW functionality
----------------------------------------
Written to page1: 0xDEADBEEF, 0x12345678
page1 physical: 0xXXXXX
Both pages marked as COW
Attempting COW write to page1...
[COW] COW write fault at 0xXXXXX
[COW] Copied page: 0xXXXXX -> 0xXXXXX
COW write successful! page1[2] = 0xCAFEBABE
page2 still has: 0xDEADBEEF, 0x12345678 (should be unchanged)

COW Statistics:
  Write faults: 1
  Pages allocated: 1
  Current blocks: 1

[Test 2] COW region registration
----------------------------------------
Allocated 4-page region at 0xXXXXX
Region registered as COW
Attempting COW write to region...
[COW] COW write fault at 0xXXXXX
COW write successful!
Current COW blocks: 4

=================================================================
Final COW Statistics:
  Total write faults:     2
  Total pages allocated:  5
  Current COW blocks:     0
  Pages coalesced:        0
=================================================================

[COW Demo] All tests completed successfully!

=================================================================
CCOS Stage 18 initialization complete!
=================================================================
```

---

## 验证要点

### 验证 COW 功能

1. **COW 页错误被正确处理**：写入 COW 页时应该触发 COW 处理
2. **引用计数正确**：统计信息中的 COW 块数量应该正确
3. **内存正确复制**：原页和副本应该有不同的内容
4. **清理正确**：测试结束后 COW 块应该被清理

### 验证异常处理

1. **Double Fault 不会 Triple Fault**：系统不应该重启
2. **GPF 打印诊断信息**：如果触发 GPF，应该看到详细的错误信息
3. **IST 栈正确配置**：TSS 初始化日志应该显示 IST 栈地址

---

## 常见编译问题

### 问题一：undefined reference to `cow_*`

**原因**：COW 模块没有被链接

**解决**：检查 CMakeLists.txt，确保 COW 模块被正确链接

### 问题二：undefined reference to `tss_get`

**原因**：TSS 模块初始化顺序问题

**解决**：确保 `tss_init()` 在 `gdt_init()` 之前调用

### 问题三：页面错误处理器不被调用

**原因**：IDT 配置问题

**解决**：检查 IDT 是否正确初始化，页错误处理器是否注册

---

## 下一步

现在我们已经完成了 Stage 18 的所有功能：

1. ✅ 写时复制模块
2. ✅ 泛型哈希表
3. ✅ 异常处理器（#DF, #SS, #GP）
4. ✅ GDT/TSS 管理
5. ✅ COW 演示程序

最后一节我们将讨论测试验证和调试技巧，包括如何使用 GDB 调试 COW、如何分析异常错误码、常见问题排查等。

在继续之前，请确保你的内核能够正常编译和运行，COW 演示程序能够成功完成所有测试。
