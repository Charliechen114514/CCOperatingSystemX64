# 06 - Bootloader 串口支持

内核的串口输出已经工作了，但 Bootloader 还是哑巴。让我们把它也加上串口支持。

---

## 我们现在要做什么

Bootloader 经历三个 CPU 模式：**实模式（16-bit）→ 保护模式（32-bit）→ 长模式（64-bit）**。

我们需要在每个模式都实现串口输出：

1. **实模式串口** — Bootloader 早期调试输出
2. **保护模式串口** — 模式切换后的输出
3. **长模式串口** — 跳转前的最后输出

---

## 第一步 —— 添加串口常量定义

打开 `boot/bootloader.asm`，在文件开头添加串口相关定义：

```asm
; =============================================================================
; 串口常量定义
; =============================================================================

SERIAL_COM1            equ 0x3F8

; 寄存器偏移
SERIAL_DATA_REG        equ 0x0   ; RBR/THR/DLL
SERIAL_INT_ENABLE_REG  equ 0x1   ; IER/DLM
SERIAL_FIFO_CTRL_REG   equ 0x2   ; FCR/IIR
SERIAL_LINE_CTRL_REG   equ 0x3   ; LCR
SERIAL_MODEM_CTRL_REG  equ 0x4   ; MCR
SERIAL_LINE_STATUS_REG equ 0x5   ; LSR

; LCR 位定义
SERIAL_LCR_DLAB        equ 0x80  ; 除数锁存访问位
SERIAL_LCR_8BIT        equ 0x03  ; 8 位数据

; FCR 值
SERIAL_FCR_ENABLE      equ 0x07  ; 启用 FIFO，清空缓冲

; MCR 值
SERIAL_MCR_ENABLE      equ 0x0B  ; 启用 RTS/DSR

; LSR 位定义
SERIAL_LSR_THRE        equ 0x20  ; 发送保持寄存器为空

; 波特率除数（115200）
SERIAL_BAUD_DLL        equ 0x01
SERIAL_BAUD_DLM        equ 0x00
```

---

## 第二步 —— 实模式串口初始化

在实模式代码段添加串口初始化函数：

```asm
; =============================================================================
; 实模式串口函数
; =============================================================================

; serial_init - 初始化 COM1 为 115200 8N1
; 输入：无
; 输出：无
; 破坏：AX, DX
bits 16
serial_init:
    push dx

    ; Step 1: 禁用中断
    mov dx, SERIAL_INT_ENABLE_REG
    xor al, al
    out dx, al

    ; Step 2: 启用 DLAB
    mov dx, SERIAL_LINE_CTRL_REG
    mov al, SERIAL_LCR_DLAB | SERIAL_LCR_8BIT
    out dx, al

    ; Step 3: 设置波特率除数
    mov dx, SERIAL_DATA_REG
    mov al, SERIAL_BAUD_DLL
    out dx, al

    mov dx, SERIAL_INT_ENABLE_REG
    mov al, SERIAL_BAUD_DLM
    out dx, al

    ; Step 4: 配置 8N1（禁用 DLAB）
    mov dx, SERIAL_LINE_CTRL_REG
    mov al, SERIAL_LCR_8BIT
    out dx, al

    ; Step 5: 启用 FIFO
    mov dx, SERIAL_FIFO_CTRL_REG
    mov al, SERIAL_FCR_ENABLE
    out dx, al

    ; Step 6: 设置调制解调器控制
    mov dx, SERIAL_MODEM_CTRL_REG
    mov al, SERIAL_MCR_ENABLE
    out dx, al

    pop dx
    ret

; serial_write_char - 发送单个字符到串口
; 输入：AL = 字符
; 输出：无
; 破坏：AX, DX
bits 16
serial_write_char:
    push dx
    push ax

.wait_transmit:
    ; 等待 THR 为空
    mov dx, SERIAL_LINE_STATUS_REG
    in al, dx
    test al, SERIAL_LSR_THRE
    jz .wait_transmit

    ; 发送字符
    pop ax
    mov dx, SERIAL_DATA_REG
    out dx, al

    pop dx
    ret

; serial_write_string - 发送字符串到串口
; 输入：SI = 字符串地址
; 输出：无
; 破坏：AX, SI
bits 16
serial_write_string:
    push ax
.loop:
    lodsb                   ; 加载 [SI] 到 AL，SI++
    test al, al             ; 检查是否为 '\0'
    jz .done
    call serial_write_char
    jmp .loop
.done:
    pop ax
    ret
```

**⚠️ 注意**：

1. 实模式使用 16 位寄存器（AX, DX, SI）
2. `LODSB` 自动加载字节并增加 SI
3. `OUT DX, AL` 格式是 `out 端口, 数据`

---

## 第三步 —— 在实模式中使用串口

现在我们在实模式的启动代码中使用串口：

```asm
bits 16
start:
    ; ... 现有代码 ...

    ; 初始化串口（新添加）
    call serial_init

    ; 输出欢迎信息到 VGA
    mov si, header_msg
    call print_string

    ; 输出欢迎信息到串口（新添加）
    mov si, header_msg
    call serial_write_string

    ; 输出串口标志
    mov si, serial_ok_msg
    call serial_write_string

    ; ... 继续现有代码 ...

; 串口消息（新添加）
serial_ok_msg: db "[Serial] Bootloader initialized", 0x0D, 0x0A, 0
```

**⚠️ 注意回车换行**：

- Windows/DOS: `\r\n` (0x0D, 0x0A)
- Unix: `\n` (0x0A)
- 串口终端通常期望 `\r\n` 才能正确换行

---

## 第四步 —— 保护模式串口

进入保护模式后，我们需要重新实现串口函数（32 位版本）：

```asm
; =============================================================================
; 保护模式串口函数
; =============================================================================

bits 32

; serial_init_32 - 保护模式串口初始化
; 输入：无
; 输出：无
; 破坏：EAX, EDX
serial_init_32:
    push edx

    ; Step 1: 禁用中断
    mov dx, SERIAL_INT_ENABLE_REG
    xor al, al
    out dx, al

    ; Step 2: 启用 DLAB
    mov dx, SERIAL_LINE_CTRL_REG
    mov al, SERIAL_LCR_DLAB | SERIAL_LCR_8BIT
    out dx, al

    ; Step 3: 设置波特率
    mov dx, SERIAL_DATA_REG
    mov al, SERIAL_BAUD_DLL
    out dx, al

    mov dx, SERIAL_INT_ENABLE_REG
    mov al, SERIAL_BAUD_DLM
    out dx, al

    ; Step 4: 配置 8N1
    mov dx, SERIAL_LINE_CTRL_REG
    mov al, SERIAL_LCR_8BIT
    out dx, al

    ; Step 5: 启用 FIFO
    mov dx, SERIAL_FIFO_CTRL_REG
    mov al, SERIAL_FCR_ENABLE
    out dx, al

    ; Step 6: 设置调制解调器控制
    mov dx, SERIAL_MODEM_CTRL_REG
    mov al, SERIAL_MCR_ENABLE
    out dx, al

    pop edx
    ret

; serial_write_string_32 - 保护模式字符串输出
; 输入：ESI = 字符串地址
; 输出：无
; 破坏：EAX, ESI
serial_write_string_32:
    push eax
.loop:
    lodsb                   ; 加载 [ESI] 到 AL，ESI++
    test al, al
    jz .done
    ; ... 发送字符（类似实模式版本）...
    jmp .loop
.done:
    pop eax
    ret
```

**16 位 vs 32 位差异**：

| 特性 | 16 位实模式 | 32 位保护模式 |
|------|------------|--------------|
| 寄存器 | AX, DX, SI | EAX, EDX, ESI |
| 指令前缀 | `bits 16` | `bits 32` |
| 地址大小 | 16 位 | 32 位 |

---

## 第五步 —— 长模式串口

进入长模式后，我们需要 64 位版本：

```asm
; =============================================================================
; 长模式串口函数
; =============================================================================

bits 64

; serial_init_64 - 长模式串口初始化
; 输入：无
; 输出：无
; 破坏：RAX, RDX
serial_init_64:
    push rdx

    ; Step 1: 禁用中断
    mov dx, SERIAL_INT_ENABLE_REG
    xor al, al
    out dx, al

    ; Step 2-6: 与 32 位版本相同的逻辑
    ; （省略，与保护模式相同）
    ; ...

    pop rdx
    ret

; serial_write_string_64 - 长模式字符串输出
; 输入：RSI = 字符串地址
; 输出：无
; 破坏：RAX, RSI
serial_write_string_64:
    push rax
.loop:
    lodsb                   ; 在 64 位模式使用 RSI
    test al, al
    jz .done
    call serial_write_char_64
    jmp .loop
.done:
    pop rax
    ret

; serial_write_char_64 - 长模式字符输出
; 输入：AL = 字符
; 输出：无
; 破坏：RAX, RDX
serial_write_char_64:
    push rdx
    push rax

.wait_transmit:
    mov dx, SERIAL_LINE_STATUS_REG
    in al, dx
    test al, SERIAL_LSR_THRE
    jz .wait_transmit

    pop rax
    mov dx, SERIAL_DATA_REG
    out dx, al

    pop rdx
    ret
```

**⚠️ 64 位特殊之处**：

1. `LODSB` 自动使用 RSI（64 位地址）
2. 端口 I/O 仍然使用 16 位 DX 寄存器
3. 需要保存被调用者保存的寄存器

---

## 第六步 —— 调用约定差异

不同模式有不同的调用约定，需要注意：

| 模式 | 参数传递 | 返回值 | 栈操作 |
|------|---------|--------|--------|
| 16 位实模式 | 寄存器/栈 | 寄存器 | PUSH/POP 16 位 |
| 32 位保护模式 | 栈（cdecl） | EAX | PUSH/POP 32 位 |
| 64 位长模式 | 寄存器（fastcall） | RAX | PUSH/POP 64 位 |

**我们的简化约定**：

由于 Bootloader 是汇编代码，我们统一使用：
- 输入：指定寄存器
- 输出：指定寄存器
- 破坏：明确声明

---

## 第七步 —— 在启动流程中使用

现在我们在各个阶段添加串口输出：

```asm
; 实模式阶段
bits 16
call serial_init
mov si, msg_real_mode
call print_string       ; VGA
call serial_write_string ; 串口

; 进入保护模式
call enable_protected_mode
bits 32
call serial_init_32
mov esi, msg_protected_mode
call serial_write_string_32

; 进入长模式
call enable_long_mode
bits 64
call serial_init_64
mov rsi, msg_long_mode
call serial_write_string_64

; 准备跳转到内核
mov rsi, msg_jump_kernel
call serial_write_string_64
```

---

## 第八步 —— 编译和测试

```bash
# 构建
cmake --build build

# 运行（串口重定向）
qemu-system-x86_64 \
    -drive format=raw,file=build/boot.img \
    -nographic \
    -serial mon:stdio
```

**预期输出**：

```
=== CCOS Bootloader v1.0 ===
[Serial] Bootloader initialized
[Serial] Entering protected mode...
[Serial] Entering long mode...
[Serial] Loading kernel...
[Serial] Jumping to kernel...
=== CCOS Kernel ===
Serial port initialized at 115200 8N1
[KERNEL] Hello from CCOS kernel!
```

---

## 第九步 —— 调试技巧

如果串口不工作，可以用以下方法调试：

### 使用 Bochs 调试器

Bochs 提供更接近硬件的调试环境：

```bash
# 安装 Bochs
sudo apt install bochs bochs-x

# 创建 .bochsrc
cat > .bochsrc << EOF
megs: 32
romimage: file=/usr/share/bochs/BIOS-bochs-latest
vgaromimage: file=/usr/share/bochs/VGABIOS-lgpl-latest
ata0-master: type=disk, path=build/boot.img, mode=flat
boot: disk
display_library: x
EOF

# 启动 Bochs
bochs -q
```

### Bochs 调试命令

```
b 0x7c00          # 在启动地址设置断点
c                 # 继续执行
s                 # 单步执行
info sreg         # 显示段寄存器
r                 # 显示寄存器
xp /20bx 0x3F8    # 查看串口端口内存
```

---

## 到这里我们完成了什么

很好，到这里我们已经完成了：

1. ✅ **实模式串口初始化和输出**（16-bit）
2. ✅ **保护模式串口初始化和输出**（32-bit）
3. ✅ **长模式串口初始化和输出**（64-bit）
4. ✅ **理解了三种模式的差异**
5. ✅ **在启动流程中集成了串口输出**

现在从 Bootloader 第一行代码开始，所有输出都能通过串口看到！

---

## 接下来

串口驱动已经完整实现了。最后一篇文章我们将学习如何配置 QEMU、运行测试，以及一些调试技巧。

→ [下一篇：上板测试与验证](./07_上板测试与验证.md)


---

<div align="center">

## 文档导航

[← 让内核支持串口输出](05_让内核支持串口输出.md)  | [上板测试与验证 →](07_上板测试与验证.md)

</div>
