# 从 Makefile 到 CMake —— 为什么要迁移

说实话，Makefile 真的绷不住了。

---

## 前言：一个真实的痛点

如果你一路跟着前面的教程走过来，我们现在停在 `stage/03_unified_boots`。这个阶段我们用了一个非常简单的 Makefile，它确实能工作，编译 bootloader、加载内核、生成启动镜像，一气呵成。但是当你开始想加点东西的时候，你会发现这个 Makefile 变得越来越臃肿，越来越难以维护。

笔者自己就经历过这个过程。一开始只是想加一个简单的 Debug 版本和 Release 版本的区分，结果发现要在 Makefile 里写两套规则。后来想加个工具链检测，确保编译前 nasm 和 qemu 都装好了，结果 Makefile 根本不支持这种前置检查。最让我崩溃的是，每次加了新的源文件，都要手动去改 Makefile，而且经常因为少写了一个文件名或者路径写错了，编译失败后排查半天。

你可能会问："为什么不用自动依赖检测？" 说实话，GNU Make 确实支持自动依赖生成，但配置起来非常繁琐，而且对于汇编文件和 C 文件混编的项目，支持并不好。你还得写一堆模式匹配规则，调试起来简直让人头大。

今年都 2026 年了，如果你还在用 Makefile 管理这种级别的项目，真的会非常痛苦。我们不是在写一个简单的 hello world，我们是在写一个操作系统内核，代码量会越来越大，模块会越来越多，Makefile 的局限性会越来越明显。

---

## 我们现在在哪里

在开始迁移之前，我们先来看看当前的状态。如果你打开 `stage/03_unified_boots` 目录，你会看到一个非常朴素的 Makefile，大概长这样：

```makefile
AS = nasm
ASFLAGS = -f bin
BOOTLOADER_SRC = boot/bootloader.asm
BOOTLOADER_BIN = build/bootloader.bin

$(BOOTLOADER_BIN): $(BOOTLOADER_SRC)
    $(AS) $(ASFLAGS) $< -o $@
```

这个 Makefile 能用，真的能用。你敲 `make`，它就编译；你敲 `make run`，它就启动 QEMU。但是当你开始想加点东西的时候，问题就来了。

**每加一个新文件，都要手动改 Makefile。** 这听起来没什么大不了的，但是当你有几十个源文件的时候，这就成了噩梦。而且你还得记住每个文件的路径，万一某个文件移动了位置，Makefile 就得跟着改。

**想搞个 Debug 版本和 Release 版本？** 你要写两套规则，或者用一堆条件判断。Debug 版本要加 `-g -O0`，Release 版本要加 `-O3 -DNDEBUG`，这些都要在 Makefile 里手动配置。更糟糕的是，切换配置要重新 make clean，否则可能会用到旧的目标文件。

**想检查一下工具链有没有装对？** Makefile 不会帮你检查。它会一直编译，直到某一步突然报错说 `nasm: command not found`，这时候你才发现工具链没装好。而且 Makefile 不会告诉你缺少哪个工具，你得自己从报错信息里猜。

**想把项目分模块管理？** Makefile 的变量处理会让你想砸键盘。比如你想把 bootloader 和内核分成两个子目录，每个有自己的 Makefile，然后写一个根 Makefile 来调用它们。这听起来很简单，但实现起来你会发现 Makefile 的递归调用、变量传递、目标依赖关系管理起来非常复杂。一个不小心，就会遇到并行编译时的竞态条件，或者增量编译失效的问题。

---

## CMake 到底是什么

在我们开始迁移之前，先来说说 CMake 到底是什么。很多人第一次听到 CMake，以为它是 Make 的替代品，其实不是。

CMake 不是构建工具，它是**构建生成器**。它不直接编译你的代码，而是生成其他构建系统的配置文件。比如它可以生成 Makefile，也可以生成 Ninja 构建文件，甚至可以生成 Visual Studio 的 `.sln` 项目文件。这意味着你写一次 CMake 配置，就可以在不同平台上用不同的构建系统来编译。

这听起来好像多了一层复杂度，但实际上非常实用。你可以在 Linux 上用 Make，在 Windows 上用 Visual Studio，在 macOS 上用 Xcode，而你的 CMake 配置只需要写一次。而且 CMake 提供了很多 Makefile 难以实现或者实现起来非常麻烦的功能，比如工具链检测、自动依赖分析、多配置支持等。

---

## CMake 能解决什么痛点

让我们来具体看看 CMake 能解决我们之前遇到的哪些问题。

### 依赖检测问题

在 Makefile 里，如果你想在编译前检查工具是否存在，你得写一些 shell 脚本，然后在 Makefile 里调用它们。而且这些检查通常是在编译过程中进行的，失败了报错信息也不够友好。

CMake 提供了 `find_program` 命令，可以在配置阶段就检查工具是否存在。比如我们可以这样写：

```cmake
find_program(NASM nasm REQUIRED)
find_program(QEMU qemu-system-x86_64 REQUIRED)
find_program(PYTHON python3 REQUIRED)
```

如果任何一个工具没找到，CMake 配置阶段就会立即失败，并且给出清晰的错误提示。你不需要等到编译中途才发现工具没装。

而且 CMake 的错误提示非常友好，它会告诉你哪个工具没找到，甚至会在某些平台上提示你如何安装这个工具。这对于新手来说非常友好，不用在一堆编译错误里翻找真正的问题。

### 多配置支持问题

在 Makefile 里，要支持 Debug 和 Release 两种配置，你得写两套编译规则，或者用条件判断。而且切换配置时要 make clean，否则可能会混用不同配置的目标文件。

CMake 内置了多种构建配置，最常用的就是 Debug 和 Release。你可以通过设置 `CMAKE_BUILD_TYPE` 变量来切换配置：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

而且每种配置可以有自己独立的编译器标志。Debug 模式下加 `-g -O0`，Release 模式下加 `-O3 -DNDEBUG`，这些都可以在 CMake 里配置好，不需要手动指定。

更方便的是，你可以在同一个构建目录下切换配置，CMake 会自动重新编译受影响的文件。你不需要每次切换都 make clean，节省了大量时间。

### 模块化管理问题

Makefile 的子目录管理真的很让人头大。你要么写一个巨大的 Makefile，要么用递归 Make，但递归 Make 有很多坑，比如变量传递、并行编译问题等。

CMake 用 `add_subdirectory` 命令来添加子目录，每个子目录有自己的 `CMakeLists.txt`，负责自己的构建规则。根目录的 `CMakeLists.txt` 只需要添加这些子目录就行了：

```cmake
add_subdirectory(boot)
add_subdirectory(kernel)
```

每个子目录的构建规则是独立的，互不干扰。而且 CMake 会自动处理子目录之间的依赖关系，不需要你手动管理。这让项目结构非常清晰，也方便多人协作。

### IDE 集成问题

现代编辑器对 CMake 的支持非常好。比如 VSCode 配合 clangd，可以根据 CMake 配置自动补全、跳转定义、显示错误信息。这在写代码的时候非常方便，你不需要等到编译就能发现很多错误。

Makefile 就没有这种待遇。虽然有些编辑器也能根据 Makefile 做一些简单的解析，但远不如 CMake 的支持完善。而且 CMake 可以生成 `compile_commands.json`，这是很多语言服务器（如 clangd）的标准输入格式。

---

## 迁移前后对比

让我们用一个表格来对比一下 Makefile 和 CMake 在这个项目中的差异：

| 方面 | stage/03 (Makefile) | stage/04 (CMake) |
|------|---------------------|------------------|
| 构建系统 | 纯 Makefile | CMake + Makefile/Ninja |
| 内核语言 | 纯汇编 | 汇编入口 + C 主函数 |
| 调试支持 | 只能反汇编 | 源码级调试（保留 ELF） |
| 构建配置 | 单一配置 | Debug/Release 一键切换 |
| 工具检测 | 无，编译失败才报错 | 配置阶段自动检测 |
| IDE 支持 | 有限，基本靠手工 | clangd/VSCode 原生支持 |
| 依赖管理 | 手动维护，容易遗漏 | 自动处理源文件变化 |
| 模块化 | 单一 Makefile 或递归调用 | 清晰的子目录结构 |
| 跨平台 | 仅限类 Unix 系统 | 可生成多种构建系统 |

这个对比可能还不够直观。让我用一个实际的例子来说明。假设你加了一个新的 C 源文件 `kernel/foo.c`，并且要在内核中使用它。

在 Makefile 里，你需要：
1. 在 Makefile 里找到编译内核的规则
2. 加上 `foo.c` 到源文件列表
3. 加上 `foo.o` 到目标文件列表
4. 加上 `foo.o` 到链接命令的依赖列表
5. 如果你想让修改自动触发重新编译，还得写依赖规则

在 CMake 里，你只需要：
1. 把 `foo.c` 放到 `kernel/CMakeLists.txt` 的源文件列表里
2. 运行 `cmake --build build`

其他的，CMake 都会自动处理。它会自动发现源文件的变化，自动决定哪些文件需要重新编译，自动处理依赖关系。你不需要担心链接顺序、不需要担心目标文件更新、不需要担心增量编译失效。

---

## 我们要达成的目标

在 `stage/04_cmake_union` 这个阶段，我们要完成几件事。首先是**构建系统迁移**，从 Makefile 迁移到 CMake。这听起来是大手术，但其实没那么可怕。我们会保留现有的功能，只是用 CMake 重新实现一遍。

其次是**内核语言升级**，从纯汇编切换到汇编入口加 C 主函数。这样我们就可以用 C 语言来写内核逻辑了，开发效率会大大提高。汇编还是需要的，但只用于那些必须用汇编的部分，比如启动时的环境设置。

然后是**调试友好**，我们会保留 ELF 文件用于 GDB 调试。之前我们只能反汇编二进制文件来调试，现在可以源码级调试，看到 C 代码的变量名、函数名，可以设置断点、单步执行。

最后是**模块化架构**，建立清晰的目录结构。bootloader 在 `boot/` 目录，内核在 `kernel/` 目录，每个子目录有自己的 `CMakeLists.txt`。这样后续添加新功能会非常方便，不会让项目变得混乱。

---

## ⚠️ 迁移前的准备

在开始迁移之前，千万确保你的 `stage/03` 能正常启动。这一点真的非常重要，我必须强调。如果你是在一个有问题的系统基础上做迁移，问题会变得更复杂。

想象一下，你的 `stage/03` 本来就有 bug，然后你迁移到 CMake 后发现了问题。这时候你不知道问题是原来就有的，还是迁移过程中引入的。排查起来会非常痛苦。

所以如果你的 `stage/03` 还没跑通，先回去解决问题。我们是在一个能工作的系统基础上做迁移，不是在一堆问题上叠加新东西。等你确认 `stage/03` 完全正常了，再继续往下看。

---

## 环境验证

在开始之前，我们来确认一下工具链是否完整。这虽然不是 CMake 特有的要求，但这是一个好的习惯。在开始任何开发工作之前，先确认工具链是否正常。

```bash
# 检查 nasm
nasm -v
# 你应该看到：NASM version 2.x.x

# 检查 gcc
gcc --version
# 你应该看到：gcc (Ubuntu xx.x.x.x) xx.x.x

# 检查 ld
ld --version
# 你应该看到：GNU ld (GNU Binutils) xx.xx

# 检查 qemu-system-x86_64
qemu-system-x86_64 --version
# 你应该看到：QEMU emulator version x.x.x

# 检查 cmake
cmake --version
# 你应该看到：cmake version x.x.x
```

如果任何一个命令报错，先安装对应的工具。在 Ubuntu/Debian 上，你可以用 `sudo apt install nasm gcc binutils qemu-system-x86 cmake` 安装。在 macOS 上，你可以用 `brew install nasm gcc qemu cmake` 安装。

还有一个重要的检查：确认你的 `stage/03` 能正常运行。进入到 `stage/03_unified_boots` 目录（或者你的 stage/03 目录），运行 `make run`。你应该能看到 QEMU 窗口启动，并且屏幕上显示我们的内核输出（一个 'X' 字符）。

如果不行，停下来，先修好它。迁移到新构建系统之前，确保起点是正确的。这话说得有点啰嗦，但真的非常重要。

---

## 接下来的路

很好，到这里我们已经确认了为什么要从 Makefile 迁移到 CMake，也确认了当前的环境和工具链都准备好了。你可能已经迫不及待想开始了，但先别急，我们需要一步步来。

接下来，我们就要开始**从零创建 CMake 构建系统**了。这会是一个有趣的过程，你会发现 CMake 虽然有点啰嗦，但它的设计确实解决了 Makefile 的很多痛点。而且一旦配置好了，后续的开发会变得非常顺畅。

在下一篇里，我们会从最基础的 CMake 配置开始，逐步搭建起完整的构建系统。我们会解释每一个命令的作用，告诉你为什么要这样写，避免你照着配置却不理解原理。

准备好了吗？让我们开始吧。

---

<div align="center">

## 文档导航

[← 上板测试](../03_unified_boots/08_上板测试.md)  | [CMake基础与项目结构 →](02_CMake基础与项目结构.md)

</div>
