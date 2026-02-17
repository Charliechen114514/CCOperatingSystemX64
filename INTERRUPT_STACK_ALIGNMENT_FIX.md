# 中断处理栈对齐问题修复记录

## 问题描述

在中断使能后，系统立即触发 Divide Error (#DE) 异常：

```
[ERROR] === EXCEPTION OCCURRED ===
[ERROR] Vector: 0 - Divide Error (#DE)
[ERROR] RIP: 0x00000000000100a1
...
```

## 根本原因

**栈未按 x86-64 ABI 要求进行 16 字节对齐。**

### 问题分析

x86-64 System V ABI 要求在调用 C 函数时，栈指针必须 16 字节对齐。如果栈不对齐，编译器生成的代码（特别是使用 SSE/AVX 指令的优化代码）可能会访问错误的内存位置，导致各种异常。

### 栈布局计算

进入 `interrupt_common` 时的栈布局：

1. **ISR stub 推送**（16 字节）：
   - error_code / dummy: 8 字节
   - vector number: 8 字节

2. **CPU 推送**（内核态 24 字节）：
   - RIP: 8 字节
   - CS: 8 字节
   - RFLAGS: 8 字节

**总计**：16 + 24 = 40 字节

40 % 16 = 8 **未对齐！**

## 修复方案

在每个 ISR stub 中额外推送 8 字节用于对齐：

### 修复前（错误）

```asm
%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    push qword 0       ; dummy error code
    push qword %1      ; vector number
    jmp interrupt_common
%endmacro
```

### 修复后（正确）

```asm
%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    push qword 0       ; dummy error code
    push qword %1      ; vector number
    push qword 0       ; alignment dummy - 确保 16 字节对齐
    jmp interrupt_common
%endmacro
```

## 验证对齐

修复后的栈布局：

1. **ISR stub 推送**（24 字节）：
   - error_code: 8 字节
   - vector: 8 字节
   - alignment: 8 字节

2. **CPU 推送**（24 字节）：
   - RIP, CS, RFLAGS

**总计**：24 + 24 = 48 字节

48 % 16 = 0 **已对齐！** ✓

## 相关修改文件

- `kernel/interrupt/interrupt.asm`
  - `ISR_NOERRCODE` 宏：添加对齐推送
  - `ISR_ERRCODE` 宏：添加对齐推送
  - `IRQ` 宏：添加对齐推送
  - `interrupt_common`：更新栈偏移计算（+8 字节）
  - 清理代码：`add rsp, 24`（原 16）

## 经验总结

1. **中断入口必须确保栈对齐**：这是 x86-64 ABI 的硬性要求
2. **不对齐的症状可能很奇怪**：如 Divide Error 而非预期的异常
3. **计算栈偏移时要考虑所有推送**：
   - ISR stub 的推送
   - CPU 自动推送的内容
   - 是否发生特权级改变（CPL change）
4. **调试技巧**：
   - 检查 RIP 是否在循环中（可能是中断返回后崩溃）
   - 检查 RSP 是否接近栈顶（可能栈溢出）
   - 反汇编确认指令是否会导致该异常

## 参考资源

- x86-64 System V ABI 规范
- Intel SDM Vol. 3A - Interrupt and Exception Handling
