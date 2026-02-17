# Makefile 自动构建省心省力

## 前言：手动命令太累了

如果你跟着前面的教程做，每次编译都要敲这一堆命令：

```bash
nasm -f bin boot/boot.asm -o build/boot.bin
nasm -f bin boot/boot2.asm -o build/boot2.bin
nasm -f bin kernel/kernel.asm -o build/kernel.bin
dd if=build/boot.bin of=build/boot.img bs=512 count=1
dd if=build/boot2.bin of=build/boot.img bs=512 seek=1 conv=notrunc
dd if=build/kernel.bin of=build/boot.img bs=512 seek=3 conv=notrunc
```

说实话，第一次觉得新鲜，第二次觉得麻烦，第三次就想"能不能自动点？"而且这些命令的参数顺序很容易记错，每次都要翻回去看文档。这就是 Makefile 的作用——一键编译，省心省力。

---

## Makefile 是什么

Makefile 是一个构建脚本，告诉 make 工具怎么编译你的项目。它的基本结构很简单：一个目标依赖于一些文件，如果依赖文件变了，就运行一些命令来更新目标。这个结构看起来像这样：

```makefile
目标: 依赖
	命令
```

目标是你想生成的文件，依赖是生成目标需要的文件，命令是生成目标的命令。注意命令前面必须是 Tab，不是空格！这个细节搞错了会导致 make 报错。

来看一个具体的例子：

```makefile
build/boot.bin: boot/boot.asm
	nasm -f bin boot/boot.asm -o build/boot.bin
```

这个规则的意思是："如果 `boot/boot.asm` 变了，就运行 `nasm ...` 生成 `build/boot.bin`"。make 会比较文件的时间戳，如果源文件比目标文件新，就重新编译。这就是"增量编译"的基础。

---

## 创建我们的 Makefile

在项目根目录创建 `Makefile`。首先是变量定义部分，这让我们可以集中管理配置：

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
```

`:=` 是立即赋值，意味着右边的值在定义时就被计算并替换。这对于简单变量来说很清晰。你也可以用 `=` 延迟赋值，但那会带来一些微妙的问题，这里不展开说。

接下来是目标定义：

```makefile
# ==============================================================================
# 目标
# ==============================================================================

.PHONY: all clean run debug

all: $(BOOT_IMG)
```

`.PHONY` 告诉 make 这些"目标"不是真实的文件。这一点很重要，因为如果你有个目录叫 `clean`，make 会混淆。声明为 `.PHONY` 后，make 总是执行这个目标，不管有没有叫这个名字的文件。

`all` 是默认目标，你运行 `make` 时它会被执行。它依赖于 `$(BOOT_IMG)`，所以 make 会先去构建 `boot.img`。

---

## 核心构建规则

现在我们定义构建 `boot.img` 的规则：

```makefile
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
```

这个规则说：`boot.img` 依赖于三个 bin 文件，如果任何一个变了，就重新生成 `boot.img`。`$@` 是自动变量，代表当前目标（`boot.img`）。`$<` 代表第一个依赖，`$^` 代表所有依赖。这些自动变量让规则更通用、更容易复制粘贴。

`@` 符号告诉 make 不要打印命令本身，只打印输出。这样输出更干净。`2>/dev/null` 重定向 stderr，抑制 dd 的"复制了 xxx 字节"的输出。

然后是各个 bin 文件的规则：

```makefile
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
```

这里有个新东西：`| prepare`。`|` 后面的是"order-only"依赖，意思是 `prepare` 必须先存在，但 `prepare` 的修改时间不影响是否重新构建当前目标。这样我们确保 `build` 目录存在，但不会因为目录的时间戳变化而导致不必要的重新编译。

---

## 运行和调试目标

除了构建，我们还定义一些方便的目标：

```makefile
run: $(BOOT_IMG)
	@echo "Starting QEMU..."
	$(QEMU) -drive format=raw,file=$(BOOT_IMG) -nographic

debug: $(BOOT_IMG)
	@echo "Starting QEMU with GDB server on :1234"
	@echo "Connect with: gdb -ex 'target remote :1234'"
	$(QEMU) -drive format=raw,file=$(BOOT_IMG) -s -S -nographic

clean:
	@echo "Cleaning..."
	rm -rf $(BUILD_DIR)
	@echo "Done."
```

`run` 目标直接启动 QEMU，`debug` 目标启动 QEMU 并暂停，等待 GDB 连接。`clean` 目标删除 `build` 目录，清理所有生成的文件。

---

## 使用 Makefile

现在编译变得非常简单：

```bash
$ make
mkdir -p build
nasm -f bin boot/boot.asm -o build/boot.bin
Boot stage 1: build/boot.bin
nasm -f bin boot/boot2.asm -o build/boot2.bin
Boot stage 2: build/boot2.bin
nasm -f bin kernel/kernel.asm -o build/kernel.bin
Kernel: build/kernel.bin
=========================================
Combining into boot image...
=========================================
Boot image created: build/boot.img
-rw-r--r-- 1 user user 2.0K Feb 17 00:10 build/boot.img
=========================================
```

运行也只需要一行：

```bash
$ make run
Starting QEMU...
qemu-system-x86_64 -drive format=raw,file=build/boot.img -nographic
```

清理也很简单：

```bash
$ make clean
Cleaning...
rm -rf build
Done.
```

如果只改了 `kernel.asm`，make 只会重新编译内核：

```bash
$ make
nasm -f bin kernel/kernel.asm -o build/kernel.bin
Kernel: build/kernel.bin
=========================================
Combining into boot image...
=========================================
```

这就是增量编译的威力——只重新编译改变了的部分，节省时间。

---

## 添加文件大小检查

为了防止文件超过扇区大小导致覆盖问题，我们添加检查规则：

```makefile
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
```

这里我们用了 `printf` 来对齐输出，`wc -c` 来获取文件字节数，`test` 来检查大小。如果文件太大，打印错误并退出（返回非零状态码）。然后修改 `all` 目标来包含大小检查：

```makefile
all: check-size $(BOOT_IMG)
```

现在运行 `make` 时会先检查大小，如果有问题会提前告诉你：

```bash
$ make check-size
=========================================
File size check:
=========================================
Stage 1:   512 / 512 bytes  [OK]
Stage 2:   743 / 1024 bytes [OK]
Kernel:     20 / 512 bytes  [OK]
=========================================
```

如果文件太大：

```bash
$ make check-size
=========================================
File size check:
=========================================
Stage 1:   512 / 512 bytes  [OK]
Stage 2:  1200 / 1024 bytes [FAIL]
=========================================
ERROR: Stage 2 too large!
make: *** [check-size] Error 1
```

---

## 更多有用的目标

我们可以添加更多目标来辅助开发。反汇编目标可以把编译后的二进制转回汇编代码，方便查看实际生成了什么：

```makefile
disasm: $(BOOT_BIN) $(BOOT2_BIN) $(KERNEL_BIN)
	@echo "Disassembling..."
	ndisasm -b 16 -o 0x7C00 $(BOOT_BIN) > $(BUILD_DIR)/boot.dis
	ndisasm -b 16 -o 0x7E00 $(BOOT2_BIN) > $(BUILD_DIR)/boot2.dis
	ndisasm -b 64 -o 0x10000 $(KERNEL_BIN) > $(BUILD_DIR)/kernel.dis
	@echo "Disassembly in $(BUILD_DIR)/*.dis"
```

`-o` 参数指定代码的起始地址，这对于反汇编很重要，因为跳转目标的计算依赖于这个地址。`-b` 指定位数（16 或 64）。

十六进制查看目标可以把二进制文件以十六进制形式显示：

```makefile
hex: $(BOOT_IMG)
	@echo "Hex dump of boot.img:"
	xxd $(BOOT_IMG) | head -40
	@echo "..."
	@echo "(Use 'xxd $(BOOT_IMG) | less' for full output)"
```

`xxd` 是一个十六进制查看工具，`head -40` 只显示前 40 行，避免输出太多。

---

## dd 命令详解

`dd` 是一个强大的工具，但参数有点反直觉。我们来详细解释一下：

```bash
dd if=build/boot.bin of=build/boot.img bs=512 count=1 conv=notrunc
```

`if=` 是输入文件，`of=` 是输出文件，`bs=` 是块大小，`count=` 是要复制的块数，`conv=notrunc` 是不截断输出文件。这个命令的意思是：从 `build/boot.bin` 读取数据，以 512 字节为单位，复制 1 个块到 `build/boot.img`，并且不要截断输出文件。

`seek` 参数是跳过输出文件的块数。`seek=3` 意味着"在输出文件中跳过 3 个块再开始写入"。这样我们就可以把内核写到扇区 3，而不会覆盖前面的 bootloader。

为什么不用 `skip`？`skip` 是跳过输入文件的块，`seek` 是跳过输出文件的块。如果想从输入文件的第 10 个块开始读，用 `skip=10`。如果想从输出文件的第 10 个块开始写，用 `seek=10`。

---

## 调试技巧

除了 GDB，我们还可以用 QEMU Monitor 来调试。用图形模式启动 QEMU：

```bash
$ qemu-system-x86_64 -drive format=raw,file=build/boot.img
```

启动后按 `Ctrl+Alt+2` 进入 monitor，然后可以执行各种命令：

```
(qemu) info registers
(qemu) xp /20hx 0x7C00
(qemu) xp /20hx 0x10000
```

`info registers` 显示所有寄存器的值，`xp` 是检查物理内存。这对于调试 bootloader 非常有用，因为此时虚拟地址映射还没完全建立。

如果你想用串口调试，可以加 `-serial stdio` 参数：

```bash
$ qemu-system-x86_64 -drive format=raw,file=build/boot.img -nographic -serial stdio
```

这样串口输出会重定向到标准输入/输出，方便查看。当然，你的 bootloader 需要支持串口输出。

---

## 容易踩的坑

说几个我踩过的坑。

Tab vs 空格是 Makefile 的新手噩梦。命令前必须是 Tab，不能用空格！如果你的编辑器把 Tab 转成空格（比如 VS Code 的"Insert Spaces"模式），Make 会报错："missing separator"。这个问题很隐蔽，因为 Tab 和空格看起来一样。一个解决办法是显式显示空白字符，或者配置编辑器在 Makefile 中不展开 Tab。

`dd` 的 `count` 参数是块数，不是字节数。`count=1` 意味着复制 1 个块（如果 `bs=512`，就是 512 字节），不是 1 个字节。我第一次用的时候以为 `count=512` 会复制 512 字节，结果复制了 256KB，差点把磁盘写满。

文件大小检查很重要！如果 Stage 2 超过 1024 字节，但 `dd seek=1` 只预留了 2 个扇区（1024 字节），会覆盖内核！这个问题很隐蔽，因为覆盖可能不会立即导致崩溃，而是在某个奇怪的地方出错。所以一定要检查文件大小，在开发阶段就发现问题。

---

## 总结

到这里，我们有了一个完整的 Makefile，编译一行命令搞定：

```bash
$ make run
```

这就是自动化构建的魅力。你不需要记住那些复杂的 dd 命令，不需要每次手动敲一堆 nasm 命令，不需要担心忘记重新编译某个文件。make 会帮你处理这一切。

说实话，一个好的 Makefile 能大大提高开发效率。刚开始可能觉得写 Makefile 很麻烦，但一旦写好了，后面就是一劳永逸。而且 Makefile 本质上是一种领域特定语言（DSL），学会之后在其他项目中也能用。

下一步，我们会把 `boot.asm` 和 `boot2.asm` 合并成一个 `bootloader.asm`，这样维护起来更方便。两个文件不如一个文件，编译一次比编译两次省事。

**下篇预告**：《合并 bootloader 是大势所趋》—— 两个文件不如一个文件。


---

<div align="center">

## 文档导航

[← 终于可以跳到内核了](04_终于可以跳到内核了.md)  | [合并bootloader是大势所趋 →](06_合并bootloader是大势所趋.md)

</div>
