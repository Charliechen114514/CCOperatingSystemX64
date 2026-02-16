# 01 - 为什么要迁移到 CMake

说实话，Makefile 真的绷不住了。

---

## 我们现在在哪里

如果你一路跟着前面的教程走过来，我们现在在 `stage/03_unified_boots`。这个阶段我们用了一个非常简单的 Makefile：

```makefile
AS = nasm
ASFLAGS = -f bin
BOOTLOADER_SRC = boot/bootloader.asm
BOOTLOADER_BIN = build/bootloader.bin

$(BOOTLOADER_BIN): $(BOOTLOADER_SRC)
    $(AS) $(ASFLAGS) $< -o $@
```

这个 Makefile 能用，真的能用。但是当你开始想加点东西的时候，你会发现：

**每加一个新文件，都要手动改 Makefile。**

**想搞个 Debug 版本和 Release 版本？你要写两套规则。**

**想检查一下工具链有没有装对？Makefile 不会帮你检查，构建到一半报错才告诉你 nasm 没找到。**

**想把项目分模块管理？Makefile 的变量处理会让你想砸键盘。**

---

## Makefile 的痛点

让我细数一下 Makefile 在这个项目里的问题：

1. **依赖检测不完善** — 你改了个头文件，Makefile 不会自动重新编译依赖它的文件
2. **构建配置单一** — 想要 Debug/Release 两种配置？手写两套规则吧
3. **工具链没有检测** — 构建到一半才发现 `qemu-system-x86_64` 没安装
4. **跨平台能力弱** — 虽然 OS 目标是 x86_64，但开发者可能在 Linux、macOS、WSL 上构建
5. **IDE 集成差** — 现代编辑器对 CMake 支持更好，能自动补全、跳转

---

## CMake 能解决什么

CMake 不是构建工具，它是**构建生成器**。它生成各种构建系统（Makefile、Ninja、Visual Studio 等）。

### CMake 的优势

| 问题 | CMake 的解决方案 |
|------|-----------------|
| 依赖检测 | `add_subdirectory()` 自动处理子目录，源文件变化自动扫描 |
| 多配置支持 | 内置 Debug/Release/MinSizeRel 等配置，`CMAKE_BUILD_TYPE` 切换 |
| 工具链检测 | `find_program()` 在构建前检查工具是否存在 |
| 模块化 | 清晰的目录结构，每个子目录有自己的 CMakeLists.txt |
| 生成器表达式 | `$<COMPILE_LANGUAGE:C>` 只对 C 文件应用特定标志 |

### 一个例子：工具链检测

在 CMake 里，我们可以这样检查工具：

```cmake
find_program(NASM nasm REQUIRED)
find_program(QEMU qemu-system-x86_64 REQUIRED)

message(STATUS "Found NASM: ${NASM}")
message(STATUS "Found QEMU: ${QEMU}")
```

如果工具没找到，构建**立即失败**，不会等到构建中途才报错。

---

## 我们要达成的目标

在这个阶段（`stage/04_cmake_union`），我们要完成：

1. **构建系统迁移** — 从 Makefile 迁移到 CMake
2. **内核语言升级** — 从纯汇编切换到 C 语言
3. **调试友好** — 保留 ELF 文件用于 GDB 调试
4. **模块化架构** — 清晰的目录结构，便于后续扩展

### 迁移前后对比

| 方面 | stage/03 (Makefile) | stage/04 (CMake) |
|------|---------------------|------------------|
| 构建系统 | Makefile | CMake + Makefile/Ninja |
| 内核语言 | 纯汇编 | 汇编入口 + C 主函数 |
| 调试支持 | 只能反汇编 | 源码级调试 |
| 构建配置 | 单一 | Debug/Release |
| 工具检测 | 无 | 自动检测 |
| IDE 支持 | 有限 | clangd/VSCode 原生支持 |

---

## ⚠️ 注意

**千万确保你的 stage/03 能正常启动，否则迁移后问题难以定位。**

如果 stage/03 还没跑通，先回去解决问题。我们是在一个能工作的系统基础上做迁移，不是在一堆问题上叠加新东西。

---

## 环境准备验证

在开始之前，我们先确认一下工具链是否完整：

```bash
# 检查 nasm
nasm -v
# 输出应该是：NASM version 2.x.x

# 检查 gcc
gcc --version
# 输出应该是：gcc (Ubuntu xx.x.x.x) xx.x.x

# 检查 ld
ld --version
# 输出应该是：GNU ld (GNU Binutils) xx.xx

# 检查 qemu-system-x86_64
qemu-system-x86_64 --version
# 输出应该是：QEMU emulator version x.x.x

# 检查 cmake
cmake --version
# 输出应该是：cmake version x.x.x
```

如果任何一个命令报错，先安装对应的工具：

```bash
# Ubuntu/Debian
sudo apt install nasm gcc binutils qemu-system-x86 cmake

# macOS (使用 Homebrew)
brew install nasm gcc qemu cmake
```

---

## 验证 stage/03 能正常运行

```bash
cd stage/03_unified_boots  # 或你的 stage/03 目录
make run
```

你应该能看到 QEMU 窗口启动，并且屏幕上显示我们的内核输出（一个 'X' 字符）。

如果不行，停下来，先修好它。迁移到新构建系统之前，确保起点是正确的。

---

## 我们下一步做什么

很好，到这里我们已经确认了：

1. 为什么要从 Makefile 迁移到 CMake
2. 当前 stage/03 的限制
3. 工具链已经准备好
4. stage/03 能正常工作

接下来，我们就要开始**从零创建 CMake 构建系统**了。这会是一个有趣的过程，你会发现 CMake 虽然有点啰嗦，但它的设计确实解决了 Makefile 的很多痛点。

---

**下一篇**：[02 - CMake 基础与项目结构](./02_CMake基础与项目结构.md)


---

<div align="center">

## 文档导航

[← 上板测试](../03_unified_boots/08_上板测试.md)  | [CMake基础与项目结构 →](02_CMake基础与项目结构.md)

</div>
