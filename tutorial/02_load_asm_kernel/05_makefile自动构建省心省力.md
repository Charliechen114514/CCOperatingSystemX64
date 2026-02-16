# Makefile 自动构建省心省力

## 前言：手动命令太累了

如果你跟着前面的教程做，每次编译都要敲：

```bash
nasm -f bin boot/boot.asm -o build/boot.bin
nasm -f bin boot/boot2.asm -o build/boot2.bin
nasm -f bin kernel/kernel.asm -o build/kernel.bin
dd if=build/boot.bin of=build/boot.img bs=512 count=1
dd if=build/boot2.bin of=build/boot.img bs=512 seek=1 conv=notrunc
dd if=build/kernel.bin of=build/boot.img bs=512 seek=3 conv=notrunc
```

说实话，第一次觉得新鲜，第二次觉得麻烦，第三次就想"能不能自动点？"

这就是 Makefile 的作用 —— 一键编译，省心省力。

---

## 第一步：理解 Makefile 基础

### Makefile 的结构

```makefile
目标: 依赖
	命令
```

- **目标**：要生成的文件
- **依赖**：生成目标需要的文件
- **命令**：生成目标的命令（注意：前面是 Tab，不是空格！）

### 示例

```makefile
build/boot.bin: boot/boot.asm
	nasm -f bin boot/boot.asm -o build/boot.bin
```

意思是："如果 `boot/boot.asm` 变了，就运行 `nasm ...` 生成 `build/boot.bin`"

---

## 第二步：创建 Makefile

在项目根目录创建 `Makefile`：

```makefile
# ==============================================================================
# CCOS Makefile
# ==============================================================================

# 目录
BOOT_DIR := boot
KERNEL_DIR := kernel
BUILD_DIR := build

# 工具
AS := nasm
DD := dd
QEMU := qemu-system-x86_64

# 源文件
BOOT_ASM := $(BOOT_DIR)/boot.asm
BOOT2_ASM := $(BOOT_DIR)/boot2.asm
KERNEL_ASM := $(KERNEL_DIR)/kernel.asm

# 输出文件
BOOT_BIN := $(BUILD_DIR)/boot.bin
BOOT2_BIN := $(BUILD_DIR)/boot2.bin
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
BOOT_IMG := $(BUILD_DIR)/boot.img

# 汇编器标志
ASFLAGS := -f bin

# ==============================================================================
# 目标
# ==============================================================================

.PHONY: all clean run debug

all: $(BOOT_IMG)

$(BOOT_IMG): $(BOOT_BIN) $(BOOT2_BIN) $(KERNEL_BIN)
	@echo "Combining into boot image..."
	dd if=$(BOOT_BIN) of=$@ bs=512 count=1 conv=notrunc
	dd if=$(BOOT2_BIN) of=$@ bs=512 seek=1 conv=notrunc
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=3 conv=notrunc
	@echo "Boot image created: $@"
	@ls -lh $@

$(BOOT_BIN): $(BOOT_ASM) | prepare
	$(AS) $(ASFLAGS) $< -o $@
	@echo "Boot stage 1: $@"

$(BOOT2_BIN): $(BOOT2_ASM) | prepare
	$(AS) $(ASFLAGS) $< -o $@
	@echo "Boot stage 2: $@"

$(KERNEL_BIN): $(KERNEL_ASM) | prepare
	$(AS) $(ASFLAGS) $< -o $@
	@echo "Kernel: $@"

prepare:
	@mkdir -p $(BUILD_DIR)

run: $(BOOT_IMG)
	$(QEMU) -drive format=raw,file=$(BOOT_IMG) -nographic

debug: $(BOOT_IMG)
	$(QEMU) -drive format=raw,file=$(BOOT_IMG) -s -S

clean:
	@echo "Cleaning build files..."
	rm -rf $(BUILD_DIR)
	@echo "Done."
```

---

## 第三步：理解 Makefile 的各个部分

### 变量定义

```makefile
BOOT_DIR := boot
BUILD_DIR := build
AS := nasm
```

`:=` 是立即赋值，`=` 是延迟赋值。对于简单变量，用 `:=` 更清晰。

### 自动变量

```makefile
$(BOOT_BIN): $(BOOT_ASM) | prepare
	$(AS) $(ASFLAGS) $< -o $@
```

- `$<`：第一个依赖（`$(BOOT_ASM)`）
- `$@`：目标（`$(BOOT_BIN)`）

这样写的好处是复制粘贴方便，改目标名时不用改命令。

### 伪目标

```makefile
.PHONY: all clean run debug
```

告诉 make 这些"目标"不是文件，所以每次都执行。

### order-only 依赖

```makefile
$(BOOT_BIN): $(BOOT_ASM) | prepare
```

`| prepare` 表示 `prepare` 是"order-only"依赖：
- `prepare` 必须先存在
- 但 `prepare` 的修改时间不影响 `$(BOOT_BIN)` 是否重新构建

---

## 第四步：使用 Makefile

现在编译变得非常简单：

```bash
# 编译所有
$ make
mkdir -p build
nasm -f bin boot/boot.asm -o build/boot.bin
Boot stage 1: build/boot.bin
nasm -f bin boot/boot2.asm -o build/boot2.bin
Boot stage 2: build/boot2.bin
nasm -f bin kernel/kernel.asm -o build/kernel.bin
Kernel: build/kernel.bin
Combining into boot image...
1+0 records in
1+0 records out
512 bytes copied, 0.00012345 s, 4.1 MB/s
...
Boot image created: build/boot.img
-rw-r--r-- 1 user user 2.0K Feb 17 00:10 build/boot.img

# 运行
$ make run
qemu-system-x86_64 -drive format=raw,file=build/boot.img -nographic

# 清理
$ make clean
Cleaning build files...
rm -rf build
Done.
```

### 增量编译

如果你只改了 `kernel.asm`：

```bash
$ make
nasm -f bin kernel/kernel.asm -o build/kernel.bin
Kernel: build/kernel.bin
Combining into boot image...
```

只有内核被重新编译！

---

## 第五步：添加文件大小检查

为了防止文件超过扇区大小，我们添加检查规则：

```makefile
# 在 Makefile 中添加

check-size: $(BOOT_BIN) $(BOOT2_BIN) $(KERNEL_BIN)
	@echo "Checking file sizes..."
	@echo "Stage 1: $$(wc -c < $(BOOT_BIN))/512"
	@echo "Stage 2: $$(wc -c < $(BOOT2_BIN))/1024"
	@echo "Kernel:  $$(wc -c < $(KERNEL_BIN))/512"
	@test $$(wc -c < $(BOOT_BIN)) -le 512 || (echo "ERROR: Stage 1 too large!" && exit 1)
	@test $$(wc -c < $(BOOT2_BIN)) -le 1024 || (echo "ERROR: Stage 2 too large!" && exit 1)
	@test $$(wc -c < $(KERNEL_BIN)) -le 512 || (echo "ERROR: Kernel too large!" && exit 1)
	@echo "All sizes OK."

# 修改 all 目标
all: check-size $(BOOT_IMG)
```

### 运行检查

```bash
$ make check-size
Checking file sizes...
Stage 1: 512/512
Stage 2: 743/1024
Kernel:  20/512
All sizes OK.
```

如果文件太大：

```bash
$ make check-size
Checking file sizes...
ERROR: Stage 2 too large!
make: *** [check-size] Error 1
```

---

## 第六步：添加更多有用的目标

### 反汇编目标

```makefile
disasm: $(BOOT_BIN) $(BOOT2_BIN) $(KERNEL_BIN)
	@echo "Disassembling Stage 1..."
	ndisasm -b 16 $(BOOT_BIN) > $(BUILD_DIR)/boot.dis
	@echo "Disassembling Stage 2..."
	ndisasm -b 16 $(BOOT2_BIN) > $(BUILD_DIR)/boot2.dis
	@echo "Disassembling Kernel..."
	ndisasm -b 64 $(KERNEL_BIN) > $(BUILD_DIR)/kernel.dis
	@echo "Disassembly in $(BUILD_DIR)/"
```

### 十六进制查看

```makefile
hex: $(BOOT_IMG)
	xxd $(BOOT_IMG) | less
```

### GDB 调试

```makefile
debug: $(BOOT_IMG)
	@echo "Starting QEMU with GDB server on :1234"
	@echo "In another terminal: gdb"
	@echo "  (gdb) target remote :1234"
	@echo "  (gdb) symbol-file kernel/kernel.elf  # 如果有符号文件"
	$(QEMU) -drive format=raw,file=$(BOOT_IMG) -s -S -nographic
```

---

## 第七步：完整的 Makefile

```makefile
# ==============================================================================
# CCOS Makefile
# ==============================================================================

# 目录
BOOT_DIR := boot
KERNEL_DIR := kernel
BUILD_DIR := build

# 工具
AS := nasm
DD := dd
QEMU := qemu-system-x86_64

# 源文件
BOOT_ASM := $(BOOT_DIR)/boot.asm
BOOT2_ASM := $(BOOT_DIR)/boot2.asm
KERNEL_ASM := $(KERNEL_DIR)/kernel.asm

# 输出文件
BOOT_BIN := $(BUILD_DIR)/boot.bin
BOOT2_BIN := $(BUILD_DIR)/boot2.bin
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
BOOT_IMG := $(BUILD_DIR)/boot.img

# 汇编器标志
ASFLAGS := -f bin

# ==============================================================================
# 目标
# ==============================================================================

.PHONY: all clean run debug check-size disasm hex

all: check-size $(BOOT_IMG)

$(BOOT_IMG): $(BOOT_BIN) $(BOOT2_BIN) $(KERNEL_BIN)
	@echo "========================================="
	@echo "Combining into boot image..."
	@echo "========================================="
	dd if=$(BOOT_BIN) of=$@ bs=512 count=1 conv=notrunc 2>/dev/null
	dd if=$(BOOT2_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=3 conv=notrunc 2>/dev/null
	@echo "Boot image created: $@"
	@ls -lh $@
	@echo "========================================="

$(BOOT_BIN): $(BOOT_ASM) | prepare
	@echo "Assembling Stage 1..."
	$(AS) $(ASFLAGS) $< -o $@

$(BOOT2_BIN): $(BOOT2_ASM) | prepare
	@echo "Assembling Stage 2..."
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL_BIN): $(KERNEL_ASM) | prepare
	@echo "Assembling Kernel..."
	$(AS) $(ASFLAGS) $< -o $@

prepare:
	@mkdir -p $(BUILD_DIR)

check-size: $(BOOT_BIN) $(BOOT2_BIN) $(KERNEL_BIN)
	@echo "========================================="
	@echo "File size check:"
	@echo "========================================="
	@printf "Stage 1: %5d / 512 bytes  " $$(wc -c < $(BOOT_BIN))
	@test $$(wc -c < $(BOOT_BIN)) -le 512 && echo "[OK]" || echo "[FAIL]"
	@printf "Stage 2: %5d / 1024 bytes " $$(wc -c < $(BOOT2_BIN))
	@test $$(wc -c < $(BOOT2_BIN)) -le 1024 && echo "[OK]" || echo "[FAIL]"
	@printf "Kernel:  %5d / 512 bytes  " $$(wc -c < $(KERNEL_BIN))
	@test $$(wc -c < $(KERNEL_BIN)) -le 512 && echo "[OK]" || echo "[FAIL]"
	@echo "========================================="
	@test $$(wc -c < $(BOOT_BIN)) -le 512 || (echo "ERROR: Stage 1 too large!" && exit 1)
	@test $$(wc -c < $(BOOT2_BIN)) -le 1024 || (echo "ERROR: Stage 2 too large!" && exit 1)
	@test $$(wc -c < $(KERNEL_BIN)) -le 512 || (echo "ERROR: Kernel too large!" && exit 1)

run: $(BOOT_IMG)
	@echo "Starting QEMU..."
	$(QEMU) -drive format=raw,file=$(BOOT_IMG) -nographic

debug: $(BOOT_IMG)
	@echo "Starting QEMU with GDB server on :1234"
	@echo "Connect with: gdb -ex 'target remote :1234'"
	$(QEMU) -drive format=raw,file=$(BOOT_IMG) -s -S -nographic

disasm: $(BOOT_BIN) $(BOOT2_BIN) $(KERNEL_BIN)
	@echo "Disassembling..."
	ndisasm -b 16 -o 0x7C00 $(BOOT_BIN) > $(BUILD_DIR)/boot.dis
	ndisasm -b 16 -o 0x7E00 $(BOOT2_BIN) > $(BUILD_DIR)/boot2.dis
	ndisasm -b 64 -o 0x10000 $(KERNEL_BIN) > $(BUILD_DIR)/kernel.dis
	@echo "Disassembly in $(BUILD_DIR)/*.dis"

hex: $(BOOT_IMG)
	@echo "Hex dump of boot.img:"
	xxd $(BOOT_IMG) | head -40
	@echo "..."
	@echo "(Use 'xxd $(BOOT_IMG) | less' for full output)"

clean:
	@echo "Cleaning..."
	rm -rf $(BUILD_DIR)
	@echo "Done."
```

---

## 第八步：dd 命令详解

`dd` 是一个强大的工具，但参数有点反直觉。

### 参数解释

```bash
dd if=build/boot.bin of=build/boot.img bs=512 count=1 conv=notrunc
```

| 参数 | 含义 |
|------|------|
| `if=` | 输入文件（input file） |
| `of=` | 输出文件（output file） |
| `bs=` | 块大小（block size） |
| `count=` | 要复制的块数 |
| `seek=` | 输出文件跳过的块数 |
| `conv=notrunc` | 不截断输出文件 |

### seek 参数

`seek=3` 意味着"在输出文件中跳过 3 个块再开始写入"。

```
块号    0      1      2      3      4
       ┌──────┬──────┬──────┬──────┬──────┐
       │ MBR  │ STG2 │ STG2 │ KNL  │      │
       └──────┴──────┴──────┴──────┴──────┘
         bs=1  seek=1       seek=3
         count=1
```

### 为什么不用 skip？

`skip` 是跳过输入文件的块，`seek` 是跳过输出文件的块。

```bash
# 从输入文件跳过 10 个块开始读
dd if=input of=output bs=512 skip=10

# 从输出文件的第 10 个块开始写
dd if=input of=output bs=512 seek=10
```

---

## 第九步：调试技巧

### QEMU Monitor

```bash
$ make run
# 按 Ctrl+A 然后 C 进入 monitor
(qemu) info registers
(qemu) xp /20hx 0x7C00
(qemu) xp /20hx 0x10000
```

### GDB + QEMU

```bash
# 终端 1
$ make debug

# 终端 2
$ gdb
(gdb) target remote :1234
(gdb) break *0x7E00
(gdb) continue
(gdb) x/20i $pc
(gdb) info registers
```

### 串口调试

如果 bootloader 支持串口（后面的版本会支持）：

```bash
$ qemu-system-x86_64 -drive format=raw,file=build/boot.img -nographic -serial stdio
```

---

## 踩坑预警

⚠️ **注意**：Tab vs 空格

Makefile 的命令前必须是 Tab，不能用空格！如果你的编辑器把 Tab 转成空格，Make 会报错："missing separator"

```makefile
# 正确
all:
	@echo "Hello"   # 前面是 Tab

# 错误
all:
    @echo "Hello"  # 前面是空格，会报错！
```

⚠️ **注意**：dd 的 count 参数

`count=1` 意味着复制 1 个块，不是 1 个字节！

⚠️ **注意**：文件大小检查

一定要检查文件大小！如果 Stage 2 超过 1024 字节，但 `dd seek=1` 只预留了 2 个扇区（1024 字节），会覆盖内核！

---

## 总结

到这里，我们：

1. ✅ 创建了一个完整的 Makefile
2. ✅ 理解了 Makefile 的语法和变量
3. ✅ 添加了文件大小检查
4. ✅ 添加了调试和反汇编目标
5. ✅ 掌握了 dd 命令的用法

现在编译一行命令搞定：

```bash
$ make run
```

这就是自动化构建的魅力！

下一步，我们会把 `boot.asm` 和 `boot2.asm` 合并成一个 `bootloader.asm`，这样维护起来更方便。

**下篇预告**：《合并 bootloader 是大势所趋》—— 两个文件不如一个文件。


---

<div align="center">

## 文档导航

[← 终于可以跳到内核了](04_终于可以跳到内核了.md)  | [合并bootloader是大势所趋 →](06_合并bootloader是大势所趋.md)

</div>
