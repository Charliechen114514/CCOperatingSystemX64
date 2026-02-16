# 搞懂 vstart 这个坑

说实话，vstart 这个机制我踩了好几天坑才完全搞明白。

---

## 问题现象

当你把 Stage 1 和 Stage 2 合并到一个文件后，你可能会遇到一些奇怪的问题：

```asm
; Stage 2 代码
section .stage2 vstart=0x7E00
    bits 16

gdt_start:
    dq 0x0000000000000000
gdt_code:
    dq 0x00CF9A000000FFFF
gdt_data:
    dq 0x00CF92000000FFFF
gdt_code64:
    dq 0x00AF9A000000FFFF
gdt_data64:
    dq 0x00CF92000000FFFF
gdt_end:

gdt_ptr:
    dw 5 * 8 - 1          ; 这里的值应该是对的
    dd gdt_start           ; 但这里可能就错了！
```

如果你把 `vstart=0x7E00` 写成 `org 0x7E00` 或者干脆不写，`gdt_start` 的值就会是错的。

然后你的系统会在 `lgdt [gdt_ptr]` 这一行崩溃，或者直接三 GDT 的位置出错。

---

## org vs vstart - 到底有什么区别？

这是关键问题。我们先来看 NASM 文档的定义：

### org

`org` 设置的是**程序计数器**的起始地址。它影响：
- `$` - 当前地址计数器
- `$$` - 当前 section 的起始地址

但 `org` **不影响标签的值**。

### vstart

`vstart` 设置的是**标签的虚拟起始地址**。它影响：
- 此 section 中所有定义的标签值

---

## 实例说明

让我们用例子来说明：

### 情况 1：使用 vstart（正确）

```asm
section .mbr
    org 0x7c00
start:
    jmp stage2

; ... 一些代码 ...

times 510-($-$$) db 0
dw 0xaa55

section .stage2 vstart=0x7E00
    bits 16
stage2:
    ; GDT 定义
gdt_start:
    dq 0
gdt_code:
    dq 0x00CF9A000000FFFF
gdt_end:

gdt_ptr:
    dw gdt_end - gdt_start - 1
    dd gdt_start    ; 这个值会是 0x7E03（正确的！）
```

在这种情况下：
- `gdt_start` 的值 = 0x7E00 + 在 .stage2 section 中的偏移
- 如果 GDT 从 .stage2 的第 3 个字节开始，`gdt_start` = 0x7E03

### 情况 2：不使用 vstart（错误）

```asm
section .mbr
    org 0x7c00
start:
    jmp stage2

; ... 一些代码 ...

times 510-($-$$) db 0
dw 0xaa55

section .stage2
    bits 16
stage2:
    ; GDT 定义
gdt_start:
    dq 0
gdt_code:
    dq 0x00CF9A000000FFFF
gdt_end:

gdt_ptr:
    dw gdt_end - gdt_start - 1
    dd gdt_start    ; 这个值会是 0x0203（错误的！）
```

在这种情况下：
- `gdt_start` 的值 = 文件偏移（没有加上 0x7E00）
- 如果 GDT 从文件的 0x203 偏移开始，`gdt_start` = 0x0203
- 但代码实际上会被加载到 0x7E00，所以 GDT 实际在 0x7E03
- 结果：GDT 指针指向错误的地址 → 系统崩溃

---

## 为什么会有这个问题？

这是因为 NASM 默认使用**文件偏移**作为标签值，而不是**运行时地址**。

在单文件编译时：
- 文件偏移 0x0000 → 运行时地址 0x7C00（MBR 段）
- 文件偏移 0x0200 → 运行时地址 0x7E00（Stage 2 段）

NASM 不知道这些映射关系，除非你用 `vstart` 告诉它。

---

## 跨 Section 引用的坑

还有一个更隐蔽的坑：**跨 section 的引用**。

```asm
section .mbr
    org 0x7c00
    bits 16
start:
    jmp stage2_entry
    ; ...

section .stage2 vstart=0x7E00
    bits 16
stage2_entry:
    ; ...

section .data    ; 新的 section！
msg:
    db "Hello", 0
```

如果你把数据放在一个新的 section 里，而又没有正确设置 `vstart`，`msg` 的地址就会是错的。

**解决方案**：把数据放在同一个 section 里：

```asm
section .stage2 vstart=0x7E00
    bits 16
stage2_entry:
    mov si, msg
    call print_bios

msg:
    db "Hello", 0    ; 在同一 section，地址正确
```

---

## 验证你的 vstart 是否正确

你可以用反汇编来验证：

```bash
# 反汇编 Stage 2 部分
ndisasm -b 16 -o 0x7E00 -e 0x200 bootloader.bin | head -50
```

`-e 0x200` 参数告诉 ndisasm 从文件偏移 0x200 开始反汇编（这是 Stage 2 的位置）。

`-o 0x7E00` 参数告诉 ndisasm 把地址显示为 0x7E00 开始。

你应该能看到类似这样的输出：

```
00007E00  EB 03              jmp short 0x7E05
00007E02  90                nop
00007E03  00 00              add [bx+si], al
...
```

如果你看到的是 0x0200 而不是 0x7E00，那说明你的 `vstart` 没起作用。

---

## bits 模式的陷阱

还有一个容易忽略的点：`bits` 指令。

```asm
section .stage2 vstart=0x7E00
    bits 16    ; 不要省略这个！
stage2_main:
    ; ...

; 后面可能有 32 位代码
bits 32
pm_entry:
    ; ...
```

`bits` 指令只影响**指令编码**，不影响标签值。但如果你省略了它，NASM 可能会用错误的模式生成指令，导致你的代码无法正常运行。

**最佳实践**：在每个 section 开始时显式声明 `bits` 模式。

---

## 常见错误和解决方法

### 错误 1：GDT 指针错误

**现象**：`lgdt [gdt_ptr]` 崩溃

**原因**：`gdt_ptr` 里的地址计算错误

**解决**：确保使用 `vstart=0x7E00`

### 错误 2：跳转目标错误

**现象**：`jmp 0x7E00` 后代码乱飞

**原因**：Stage 2 的地址偏移不对

**解决**：检查 `vstart` 和文件偏移的对应关系

### 错误 3：数据访问错误

**现象**：访问字符串或数组时读到垃圾数据

**原因**：数据放在了错误的 section，没有正确的 `vstart`

**解决**：把数据放在使用它的同一个 section 里

---

## 总结

记住这几个要点：

1. **vstart 告诉 NASM 标签的运行时地址**
   - `section .stage2 vstart=0x7E00` → 标签值 = 0x7E00 + 偏移

2. **org 设置程序计数器，不影响标签值**
   - `org 0x7c00` 主要用于 `$` 和 `$$`

3. **把数据放在正确的 section**
   - 不要随意创建新的 section

4. **显式声明 bits 模式**
   - 每个 section 开始时写上 `bits 16/32/64`

5. **用反汇编验证**
   - `ndisasm -b 16 -o 0x7E00 -e 0x200 bootloader.bin`

---

## 下一步

现在我们搞懂了 `vstart`，可以继续完善我们的 bootloader 了。

下一篇教程里，我们会加上**串口输出**，这样调试起来就方便多了。相信我，当你第一次通过串口看到 bootloader 的输出时，你会感谢自己做了这个功能。


---

<div align="center">

## 文档导航

[← 第一步：合并两个文件](02_第一步_合并两个文件.md)  | [加上串口输出 →](04_加上串口输出.md)

</div>
