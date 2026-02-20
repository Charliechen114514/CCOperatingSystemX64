# Ring 3 切换汇编实现

## 前言

终于到了最激动人心的部分 —— 我们要实现从 Ring 0 到 Ring 3 的切换了！

说实话，写这部分代码的时候我是真的很紧张。一个字节错了，整个系统就会崩溃。而且这种崩溃很难调试，因为涉及到特权级切换，常规的调试手段可能不太好用。

但这是必须迈过的一步。没有这个切换机制，我们的用户态就只是一个概念，无法真正运行。所以让我们来仔细地、一步一步地实现它。

---

## 理解 iretq 指令

在写代码之前，我们需要完全理解 `iretq` 指令是怎么工作的。

### iretq 的栈帧

当 iretq 执行特权级切换时（比如 Ring 0 → Ring 3），它期望栈上的数据是这样的：

```
┌─────────────────────────────────────────────────────────┐
│  栈顶 (低地址)        │ 内容                           │
├─────────────────────────────────────────────────────────┤
│  +0x00  │  RIP (8 bytes)    │ 返回地址                 │
│  +0x08  │  CS  (8 bytes)    │ 代码段选择器 (带 RPL)    │
│  +0x10  │  RFLAGS (8 bytes) │ 标志寄存器               │
│  +0x18  │  RSP (8 bytes)    │ 栈指针 (仅当 CPL 改变)    │
│  +0x20  │  SS  (8 bytes)    │ 栈段 (仅当 CPL 改变)      │
├─────────────────────────────────────────────────────────┤
│  栈底 (高地址)                                        │
└─────────────────────────────────────────────────────────┘
```

**关键点**：SS 和 RSP 只有在 CPL (当前特权级) 改变时才会被弹出。因为我们是从 Ring 0 切换到 Ring 3，CPL 会改变，所以需要这 5 项。

### iretq 的执行过程

当 CPU 执行 `iretq` 时：

```
1. 弹出 RIP → 指令指针
2. 弹出 CS → 代码段寄存器
   - CPU 检查 CS 的 RPL 位
   - 如果 RPL < CPL（比如 3 < 0），触发 #GP 异常
   - 如果 RPL > CPL（比如 3 > 0），切换到 Ring RPL

3. 弹出 RFLAGS → 标志寄存器

4. 因为 CPL 改变：
   - 弹出 RSP → 栈指针
   - 弹出 SS → 栈段寄存器

5. CPL 现在是 Ring 3
   - 用户代码开始执行
```

---

## 创建汇编文件

现在让我们创建 `kernel/user/user_enter.asm`：

```nasm
; ============================================================================
; user_enter.asm - User Mode Entry Point for x86_64
; ============================================================================
; This file contains the low-level assembly code for transitioning from
; kernel mode (Ring 0) to user mode (Ring 3).
; ============================================================================

section .text
bits 64

; ============================================================================
; user_context_t Structure Offsets
; ============================================================================
; These must match the user_context_t structure in user.h

struc user_context
    .entry     resq 1    ; virtual_addr_t entry (user RIP)
    .stack_top resq 1    ; virtual_addr_t stack_top (user RSP)
    .cs        resq 1    ; uint64_t cs
    .ss        resq 1    ; uint64_t ss
    .rflags    resq 1    ; uint64_t rflags
endstruc
```

⚠️ **注意**

这里的结构偏移量**必须**与 C 代码中的 `user_context_t` 结构完全匹配。如果偏移量不对，加载的值就是错的，系统会崩溃。

NASM 的 `resq 1` 表示保留 8 字节（quad word），正好对应一个 64 位的值。

---

## 实现核心函数

现在来实现 `user_switch_to_usermode` 函数：

```nasm
; ============================================================================
; user_switch_to_usermode - Switch from kernel mode to user mode
; ============================================================================
; This function performs the actual transition from Ring 0 to Ring 3.
; It does NOT return.
;
; C signature: void user_switch_to_usermode(user_context_t* ctx)
;
; Register usage (System V AMD64 ABI):
;   RDI = ctx (pointer to user_context_t)
; ============================================================================

global user_switch_to_usermode
user_switch_to_usermode:
    ; RDI contains pointer to user_context_t

    ; Load user context into registers
    mov rax, [rdi + user_context.entry]      ; RAX = entry (user RIP)
    mov rbx, [rdi + user_context.stack_top]  ; RBX = stack_top (user RSP)
    mov rcx, [rdi + user_context.cs]         ; RCX = user CS
    mov rdx, [rdi + user_context.ss]         ; RDX = user SS
    mov rsi, [rdi + user_context.rflags]     ; RSI = user RFLAGS
```

**代码解释**：

我们使用不同的寄存器来暂存这些值，因为：
- RDI 是输入参数（ctx 指针），我们需要读取它
- RAX, RBX, RCX, RDX, RSI 是调用者保存寄存器，我们可以随意使用
- 我们不需要保存这些寄存器，因为函数不会返回

---

## 设置 iretq 栈帧

现在来设置 iretq 需要的栈帧：

```nasm
    ; Set up user stack for iretq
    ; iretq expects: SS, RSP, RFLAGS, CS, RIP (pushed in that order)
    ; But we push in reverse order since stack grows down

    ; Push user SS
    push rdx                 ; SS
    ; Push user RSP
    push rbx                 ; RSP
    ; Push user RFLAGS
    push rsi                 ; RFLAGS
    ; Push user CS
    push rcx                 ; CS
    ; Push user RIP
    push rax                 ; RIP
```

**压栈顺序详解**：

记住：栈是向下增长的！最后压入的在栈顶。

压栈顺序：
1. `push rdx` (SS) → 地址最高
2. `push rbx` (RSP)
3. `push rsi` (RFLAGS)
4. `push rcx` (CS)
5. `push rax` (RIP) → 地址最低（栈顶）

当 iretq 执行时，它从栈顶开始弹出：
- 先弹出 RIP (rax)
- 然后弹出 CS (rcx)
- 然后弹出 RFLAGS (rsi)
- 然后弹出 RSP (rbx)
- 最后弹出 SS (rdx)

---

## 设置数据段寄存器

iretq 只会自动设置 CS 和 SS，其他数据段寄存器需要手动设置：

```nasm
    ; Clear all data segment registers
    ; This ensures we enter user mode with correct segments
    mov ax, 0x23             ; USER_SS = GDT_USER_DATA | 3
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
```

**为什么是 0x23？**

0x23 = 0x20 | 3
- 0x20 = GDT 索引 4（用户数据段）
- 3 = RPL (Ring 3)

如果数据段的 RPL 不是 3，用户程序访问数据时可能会有问题。

---

## 执行切换

最后，执行 iretq：

```nasm
    ; Switch to user mode via iretq
    iretq

; Never returns here
```

当 iretq 执行后：
1. CPU 切换到 Ring 3
2. 用户栈被加载
3. 用户代码开始执行
4. 内核代码不再执行

---

## 完整的汇编代码

让我们把完整的代码放在一起：

```nasm
; ============================================================================
; user_enter.asm - User Mode Entry Point for x86_64
; ============================================================================

section .text
bits 64

; ============================================================================
; user_context_t Structure Offsets
; ============================================================================

struc user_context
    .entry     resq 1
    .stack_top resq 1
    .cs        resq 1
    .ss        resq 1
    .rflags    resq 1
endstruc

; ============================================================================
; user_switch_to_usermode
; ============================================================================

global user_switch_to_usermode
user_switch_to_usermode:
    ; RDI = ctx pointer

    ; Load user context
    mov rax, [rdi + user_context.entry]
    mov rbx, [rdi + user_context.stack_top]
    mov rcx, [rdi + user_context.cs]
    mov rdx, [rdi + user_context.ss]
    mov rsi, [rdi + user_context.rflags]

    ; Set up iretq stack frame (SS, RSP, RFLAGS, CS, RIP)
    push rdx        ; SS
    push rbx        ; RSP
    push rsi        ; RFLAGS
    push rcx        ; CS
    push rax        ; RIP

    ; Set data segments
    mov ax, 0x23    ; USER_SS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Switch to Ring 3
    iretq
```

---

## 常见陷阱

这部分代码有几个特别容易出错的地方。

### 陷阱 1：栈帧顺序错误

```nasm
/* 错误的顺序 */
push rax    ; RIP
push rcx    ; CS
push rsi    ; RFLAGS
push rbx    ; RSP
push rdx    ; SS
iretq       /* 崩溃！ */
```

**正确的顺序**：SS, RSP, RFLAGS, CS, RIP（从先压栈到后压栈）

### 陷阱 2：RPL 未设置

```c
/* C 代码中错误 */
user_context_t ctx = {
    .entry = 0x400000,
    .stack_top = USER_END - 8,
    .cs = 0x18,    /* 错！RPL=0 */
    .ss = 0x20,    /* 错！RPL=0 */
    .rflags = 0x202
};
```

**正确**：
```c
.cs = 0x1B,    /* 0x18 | 3 */
.ss = 0x23,    /* 0x20 | 3 */
```

### 陷阱 3：RFLAGS IF 位未设置

```c
.rflags = 0x0,    /* 错！中断被禁用 */
```

**正确**：
```c
.rflags = 0x202,   /* IF = 1, 启用中断 */
```

0x202 的二进制是 0010 0000 0010，第 9 位（IF 位）是 1，表示启用中断。

### 陷阱 4：忘记设置数据段

如果 DS, ES, FS, GS 仍然指向内核段，用户程序访问数据时可能会触发异常。

**必须**在 iretq 之前设置：
```nasm
mov ax, 0x23
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax
```

---

## 调试技巧

### 使用 GDB 单步调试

在切换之前设置断点：

```bash
gdb build/kernel.elf
(gdb) break user_switch_to_usermode
(gdb) run
(gdb) si     # 单步执行
(gdb) info registers   # 查看寄存器
```

### 打印用户上下文

在 C 代码中添加调试输出：

```c
void debug_print_user_context(user_context_t* ctx) {
    klog_info("=== User Context ===\n");
    klog_info("RIP: 0x%llX\n", ctx->entry);
    klog_info("RSP: 0x%llX\n", ctx->stack_top);
    klog_info("CS:  0x%llX (RPL=%d)\n", ctx->cs, ctx->cs & 3);
    klog_info("SS:  0x%llX (RPL=%d)\n", ctx->ss, ctx->ss & 3);
    klog_info("RFLAGS: 0x%llX\n", ctx->rflags);
}
```

### 验证栈帧

你可以在切换前打印内核栈的内容：

```c
void debug_print_stack_before_switch(uint64_t* stack_ptr) {
    klog_info("=== Stack before iretq ===\n");
    for (int i = 0; i < 5; i++) {
        klog_info("[+%d]: 0x%llX\n", i * 8, stack_ptr[i]);
    }
}
```

---

## 测试代码

在真正切换到用户程序之前，我们可以写一个简单的测试：

```c
void test_ring3_switch(void) {
    klog_info("=== Ring 3 Switch Test ===\n");

    /* 创建一个简单的测试上下文 */
    user_context_t ctx = {
        .entry = 0x400000,              /* 测试入口点 */
        .stack_top = USER_END - 8,      /* 用户栈顶 */
        .cs = USER_CS,                  /* 0x1B */
        .ss = USER_SS,                  /* 0x23 */
        .rflags = 0x202                 /* IF = 1 */
    };

    debug_print_user_context(&ctx);

    /* 注意：这里会真的切换到 Ring 3
     * 确保 0x400000 有有效的用户代码 */
    klog_info("Switching to Ring 3...\n");
    user_switch_to_usermode(&ctx);

    /* 永远不会到这里 */
    klog_error("ERROR: Returned from Ring 3 switch!\n");
}
```

⚠️ **注意**

这个测试会真正切换到 Ring 3，所以确保在 0x400000 有有效的用户代码。否则会触发页错误或异常。

---

## CMake 配置

确保汇编文件被正确编译。在 `kernel/user/CMakeLists.txt` 中：

```cmake
set(USER_ASM_SOURCES
    user_enter.asm
)

add_library(user_asm OBJECT ${USER_ASM_SOURCES})

target_include_directories(user_asm PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kernel
)

list(APPEND KERNEL_ASM_OBJECTS
    $<TARGET_OBJECTS:user_asm>
)
```

---

## 编译验证

```bash
cd build
make -j$(nproc)
```

如果编译成功，你应该看到：

```
[ 45%] Building ASM object kernel/user/CMakeFiles/user_asm.dir/user_enter.asm.o
[100%] Linking C executable kernel.elf
```

---

## 检查清单

在继续下一篇文章之前，请确认：

- [ ] 创建了 `user_enter.asm` 文件
- [ ] `user_context` 结构偏移量与 C 代码匹配
- [ ] iretq 栈帧顺序正确 (SS, RSP, RFLAGS, CS, RIP)
- [ ] CS 和 SS 的 RPL 设置为 3
- [ ] RFLAGS 的 IF 位设置为 1
- [ ] 数据段寄存器正确设置
- [ ] 汇编文件在 CMake 中正确配置
- [ ] 编译成功，没有警告

---

## 接下来

现在我们有了 Ring 3 切换的汇编代码。在下一篇文章中，我们会实现用户进程的创建和栈管理，包括分配用户栈、映射页表等。

我们会看到：
- 如何分配 1MB 的用户栈
- 如何将用户栈映射到用户空间
- 如何扩展 PCB 结构支持用户态
- 如何创建和销毁用户进程

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← 用户态支持模块从零实现](03_用户态支持模块从零实现.md) | [用户进程创建与栈管理 →](05_用户进程创建与栈管理.md)

</div>
