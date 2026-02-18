# 15 - 内核集成与 VMM 演示程序

说实话，看到所有代码终于能跑起来的时候，那种成就感是无与伦比的。但这之前经历了无数次的编译错误、链接错误和运行时崩溃。

---

## 修改 kernel_init.c

现在我们需要把 VMM 模块集成到内核的初始化流程中。首先添加必要的头文件：

```c
#include "mm/vmm/fault.h"
#include "mm/vmm/page.h"
#include "mm/vmm/vmm.h"
```

然后在 `kernel_init` 函数中，VMM 模块的初始化必须在物理帧管理器之后。初始化顺序非常重要：`page_init` 必须在 `pframe_init` 之后，因为它需要使用物理帧分配器。`vmm_init` 必须在 `page_init` 之后，因为它需要使用页表管理功能。

```c
void kernel_init(void) {
    /* ... 其他初始化代码 ... */

    /* Initialize physical frame manager */
    pframe_init();
    pframe_dump();

    /* Initialize page table management */
    page_init();

    /* Initialize virtual memory manager */
    vmm_init();

    /* Initialize page fault handler */
    pf_init();

    /* Dump PML4 structure */
    page_dump_pml4(page_get_pml4());

    /* ... 其他初始化代码 ... */
}
```

---

## 栈保护实现

GCC 的 `-fstack-protector` 选项会在函数中插入栈保护代码，检测栈溢出。如果栈被破坏，它会调用 `__stack_chk_fail` 函数。我们需要实现这个函数：

```c
/* kernel/base/stack_check.c */

__attribute__((noreturn))
void __stack_chk_fail(void) {
    __asm__ volatile("cli");

    /* Direct VGA output */
    const char* msg = "STACK CORRUPTION DETECTED - HALTING";
    volatile uint16_t* vga_buffer = (volatile uint16_t*)0xB8000;

    /* Write to VGA in red */
    for (int i = 0; msg[i] != '\0'; i++) {
        vga_buffer[i] = (uint16_t)msg[i] | 0x4C00;  /* Red on black */
    }

    /* Halt */
    while (1) {
        __asm__ volatile("hlt");
    }
}
```

这里我们直接写 VGA 缓冲区，而不是使用 klog。因为在栈被破坏的情况下，调用 klog 可能会进一步崩溃。

---

## VMM 演示程序

VMM 演示程序展示了虚拟内存管理的各种功能。`vmm_run_demo` 函数依次调用各个演示函数：显示页表统计信息，测试地址转换，分配虚拟页面，创建用户地址空间，测试巨大页映射，以及可选的页错误测试。

```c
void vmm_run_demo(bool test_page_fault) {
    klog_info("[VMM_DEMO] Starting VMM demonstration...\n");

    /* 1. Page table statistics */
    demo_page_stats();

    /* 2. Address translation test */
    demo_address_translation();

    /* 3. Page allocation and mapping */
    demo_page_allocation();

    /* 4. User address space */
    demo_user_space();

    /* 5. Huge page mapping */
    demo_huge_pages();

    /* 6. Page fault test (optional) */
    if (test_page_fault) {
        demo_page_fault();
    }

    klog_info("[VMM_DEMO] Demonstration complete.\n");
}
```

---

## 修改 CMakeLists.txt

我们需要在 CMakeLists.txt 中添加 VMM 模块和演示程序。在 `kernel/CMakeLists.txt` 中添加：

```cmake
# Add VMM subdirectory
add_subdirectory(mm/vmm)

# Add demo subdirectory
add_subdirectory(demo)
```

在 `kernel/demo/CMakeLists.txt` 中：

```cmake
# VMM demo
if(CCOS_VMM_DEMO)
    target_compile_definitions(kernel PRIVATE VMM_DEMO_ENABLED)
endif()
```

---

## 编译验证

现在我们可以编译并运行内核：

```bash
cd /home/charliechen/CCOperatingSystemX64

# 配置构建
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 构建
cmake --build build

# 运行
qemu-system-x86_64 -drive format=raw,file=build/boot.img
```

如果一切正常，你应该能看到类似这样的输出：

```
[PAGE] Initializing with PML4 at 0x0000000000009000
[PAGE] Page size: 4096 bytes, 512 entries per table
[PAGE] Direct mapping established at PML4[256]
[PAGE]   Virt base: 0xFFFF800000000000
[PAGE]   Phys range: 0x00000000 - 0x1FFFFFFF (512MB)
[PAGE] Initialization complete
[VMM] Virtual memory manager initialized
[PF] Page fault handler registered
[VMM_DEMO] Starting VMM demonstration...
[VMM_DEMO] Page table statistics:
[VMM_DEMO]   Total mappings: XXX
[VMM_DEMO] Address translation test PASSED
[VMM_DEMO] Page allocation test PASSED
[VMM_DEMO] User space creation test PASSED
[VMM_DEMO] Huge page mapping test PASSED
[VMM_DEMO] Demonstration complete.
```

---

## 调试过程中可能会遇到的几种情况

如果遇到链接错误提示 `undefined reference to page_init`，这通常是因为 VMM 模块没有被正确链接。检查 CMakeLists.txt 中是否添加了 VMM 子目录。

如果遇到编译错误提示 `stack_chk_fail` 未定义，确保 `stack_check.c` 被添加到构建系统中。检查 `kernel/base/CMakeLists.txt`。

如果运行时崩溃，提示直接映射未建立，确保 `page_init` 在 `vmm_init` 之前调用。直接映射在 `page_init` 中建立，`vmm_init` 依赖它。

---

## 下一步

现在我们已经完成了 VMM 模块的集成和演示程序。最后一步是学习如何调试虚拟内存管理系统。在下一个文档中，我们会讲解 QEMU Monitor 和 GDB 的调试技巧，以及如何排查常见的虚拟内存问题。
