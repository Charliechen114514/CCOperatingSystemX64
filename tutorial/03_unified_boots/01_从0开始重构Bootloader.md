# 从0开始重构 Bootloader

说实话，我实在绷不住旧方案了。

---

## 前言 - 为什么要重构

如果你跟我一样，已经在这个项目上折腾了一段时间，那你一定也会发现：**两阶段 bootloader 的架构真的很烦人**。

我们之前的代码长这样：

```
boot/
├── boot.asm      # Stage 1 (MBR, 512字节)
└── boot2.asm     # Stage 2 (加载器)
```

每次修改点什么，都要在两个文件之间跳来跳去。更气人的是，这两个文件里有大量重复代码。不信你看：

```asm
; boot.asm 里的 print_string
print_string:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0e
    int 0x10
    jmp .loop
.done:
    popa
    ret

; boot2.asm 里的 print_bios (几乎一模一样!)
print_bios:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0e
    int 0x10
    jmp .loop
.done:
    popa
    ret
```

这可不是什么"代码复用"，这就是纯粹的复制粘贴。

还有构建过程，简直是噩梦：

```makefile
# 先编译 boot.asm
nasm -f bin boot.asm -o boot.bin

# 再编译 boot2.asm
nasm -f bin boot2.asm -o boot2.bin

# 然后手动拼起来
dd if=boot.bin of=boot.img bs=512 count=1
dd if=boot2.bin of=boot.img bs=512 seek=1
dd if=kernel.bin of=boot.img bs=512 seek=3
```

我受够了。今年都 2026 年了，我们还在用这种原始的构建方式。

---

## 我们的目标

我们要做的事情很清晰：

1. **单文件架构** - 把 Stage 1 和 Stage 2 合并到一个文件里
2. **消除重复代码** - 打印函数、磁盘读取函数只写一次
3. **简化构建** - 一次编译，搞定所有事情
4. **保持功能不变** - 最终效果要和之前一样

在开始之前，我们要先看一下环境：

```
平台: x86_64
汇编器: NASM 2.x
开发环境: Linux/WSL
目标: 从 MBR 启动，切换到 64 位长模式，加载内核
```

---

## 对比：重构前后

### 重构之前（两阶段）

```
文件结构:
┌─────────────┐     ┌─────────────┐
│  boot.asm   │     │ boot2.asm   │
│             │     │             │
│  Stage 1    │ ──→ │  Stage 2    │ ──→ Kernel
│  (512字节)  │     │  (~824字节) │
└─────────────┘     └─────────────┘
      │                   │
      └───── 重复代码 ─────┘

磁盘布局:
扇区 1:     boot.bin   (Stage 1)
扇区 2-3:   boot2.bin  (Stage 2)
扇区 4+:    kernel.bin

构建步骤:
1. 编译 boot.asm → boot.bin
2. 编译 boot2.asm → boot2.bin
3. 拼接到 boot.img
```

### 重构之后（统一架构）

```
文件结构:
┌─────────────────────────────┐
│      bootloader.asm         │
│                             │
│  ┌──────────┐  ┌─────────┐  │
│  │ Stage 1  │  │ Stage 2 │  │
│  │ .mbr段   │  │.stage2段│  │
│  └──────────┘  └─────────┘  │
└─────────────────────────────┘
              │
              └── 共享函数（只写一次）

磁盘布局:
扇区 1-2:   bootloader.bin  (Stage 1+2 合体)
扇区 3+:    kernel.bin

构建步骤:
1. 编译 bootloader.asm → bootloader.bin
2. 拼接到 boot.img
```

你看，简单多了。

---

## 核心技术：NASM 的 section 和 vstart

你可能会问：**把两个文件合并成一个，那 NASM 怎么知道哪段代码放在哪个地址？**

这就是 NASM 的 `section` 和 `vstart` 指令发挥作用的地方了。

```asm
; Stage 1: MBR (加载到 0x7C00)
section .mbr
    org 0x7c00
    bits 16
start:
    ; Stage 1 代码在这里
times 510-($-$$) db 0
dw 0xaa55

; Stage 2: (加载到 0x7E00)
section .stage2 vstart=0x7E00
    bits 16
stage2_entry:
    ; Stage 2 代码在这里
```

这里的 `vstart=0x7E00` 是关键。它告诉 NASM："这个 section 里的所有标签，都要加上 0x7E00 这个偏移"。

我们会在下一篇教程里详细讲解这个机制，因为它真的是个坑点。

---

## 下一步

现在我们有了明确的目标，接下来就是动手了。

下一篇教程里，我们会：
1. 把现有的 `boot.asm` 和 `boot2.asm` 合并成 `bootloader.asm`
2. 使用 `section` 指令分离两个阶段
3. 验证编译和链接是否正确

准备好了吗？让我们开始吧。


---

<div align="center">

## 文档导航

[← README](README.md)  | [第一步：合并两个文件 →](02_第一步_合并两个文件.md)

</div>
