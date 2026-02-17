# CMake 基础与项目结构

现在我们开始从零创建 CMake 构建系统。别急，我们一行一行来解释，不会让你照着配置却不理解原理。

---

## 从零开始创建 CMakeLists.txt

我们在项目根目录创建 `CMakeLists.txt`。这个文件是 CMake 的配置文件，它告诉 CMake 如何构建我们的项目。虽然它的语法看起来有点奇怪，但一旦你理解了其中的逻辑，就会发现它其实非常直观。

### 第一步：声明 CMake 版本和项目

首先，我们要告诉 CMake 我们需要的最低版本，以及我们的项目叫什么名字、用什么语言。

```cmake
cmake_minimum_required(VERSION 3.20)
project(CCOS_x64 ASM C)
```

这里有两行代码，每一行都有它的作用。`cmake_minimum_required(VERSION 3.20)` 告诉 CMake，这个配置文件需要至少 3.20 版本的 CMake 才能运行。如果有人用老版本的 CMake 来配置我们的项目，CMake 会立即报错，并给出清晰的提示，而不是在配置过程中莫名其妙地失败。

你可能会问：为什么要指定版本？因为 CMake 的不同版本之间，语法和功能是有差异的。我们用到了一些较新的特性，如果用旧版本的 CMake，可能会不支持这些特性，或者行为不一致。指定版本可以避免这些问题。

`project(CCOS_x64 ASM C)` 这行代码定义了我们的项目名称为 `CCOS_x64`，并且声明我们使用两种语言：ASM（汇编）和 C。这里要注意，ASM 必须写在前面，因为我们的项目是从汇编开始的。CMake 会根据我们声明的语言，自动检测对应的编译器。如果系统中没有找到对应的编译器，CMake 会报错。

你可能会注意到我们声明的是 `ASM` 而不是 `NASM`。这是因为 CMake 支持多种汇编器，NASM 只是其中一种。CMake 会根据平台和配置自动选择合适的汇编器。在我们的 x86_64 Linux 环境下，它会选择 NASM。

### 第二步：工具链检测

现在我们来做 Makefile 做不到的事情——在构建前检查工具是否存在。这真的非常实用，我必须强调。

```cmake
find_program(NASM nasm REQUIRED)
find_program(QEMU qemu-system-x86_64 REQUIRED)
find_program(PYTHON python3 REQUIRED)

message(STATUS "Found NASM: ${NASM}")
message(STATUS "Found QEMU: ${QEMU}")
message(STATUS "Found Python: ${PYTHON}")
```

`find_program` 是 CMake 的一个命令，用来在系统 PATH 中查找指定的程序。第一个参数是变量名，找到的程序路径会存储在这个变量里。第二个参数是程序名称，`REQUIRED` 表示这个程序是必须的，如果找不到，CMake 配置就会失败并报错。

这里有一个非常实用的地方。如果没有 `REQUIRED`，即使工具没找到，配置也会继续，只是变量会是空的。这会导致后续编译时才报错，而且错误信息可能很模糊。加上 `REQUIRED` 后，问题会在配置阶段就被发现，而且 CMake 会明确告诉你哪个工具没找到。

`message(STATUS ...)` 是 CMake 的输出语句。`STATUS` 表示这是一条普通信息，不是警告或错误。在配置时，这些信息会显示在终端上。`${NASM}` 是变量的引用方式，CMake 会把它替换成变量 `NASM` 的值。

实际运行时，你会看到类似这样的输出：

```
-- Found NASM: /usr/bin/nasm
-- Found QEMU: /usr/bin/qemu-system-x86_64
-- Found Python: /usr/bin/python3
```

如果你看到这些信息，说明工具链检测通过了。如果看到类似 "Could not find NASM" 的错误，说明你的环境有问题，需要先安装对应的工具。

### 第三步：设置构建类型

CMake 内置了几种构建配置，我们可以让用户选择使用哪一种。

```cmake
# 设置构建类型（支持 Debug/Release）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
```

`CMAKE_BUILD_TYPE` 是 CMake 的内置变量，用来控制构建类型。常用的值有 `Debug`、`Release`、`RelWithDebInfo`、`MinSizeRel`。Debug 模式下会包含调试信息，不优化；Release 模式下会优化代码，不包含调试信息。

这段代码的逻辑是：如果用户没有指定 `CMAKE_BUILD_TYPE`（即 `NOT CMAKE_BUILD_TYPE` 为真），我们就把它设为 `Release`。`CACHE STRING` 表示这个值会被缓存，下次运行 cmake 时会记住用户的设置。`FORCE` 表示即使缓存里已经有值了，也要强制覆盖。

你可能好奇为什么要这么写。这是因为 CMake 的缓存机制。第一次配置时，`CMAKE_BUILD_TYPE` 是空的，所以会设置成 `Release`。下次再配置时，`CMAKE_BUILD_TYPE` 已经有值了（从缓存读取），所以不会覆盖。用户可以通过 `-DCMAKE_BUILD_TYPE=Debug` 来指定不同的构建类型。

你可以这样切换构建类型：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

注意，`-B build` 表示构建目录是 `build`，CMake 会在 `build` 目录下生成构建文件。这是一个好习惯，把构建产物和源代码分开。

### 第四步：编译器标志

现在我们设置 C 编译器的通用选项。这部分有点长，我们慢慢来看。

```cmake
# 通用 C 编译器标志
add_compile_options(
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Wextra>
    $<$<COMPILE_LANGUAGE:C>:-Werror>
    $<$<COMPILE_LANGUAGE:C>:-std=c23>
    $<$<COMPILE_LANGUAGE:C>:-m64>
    $<$<COMPILE_LANGUAGE:C>:-mno-sse>
    $<$<COMPILE_LANGUAGE:C>:-mno-sse2>
)
```

`add_compile_options` 是 CMake 的命令，用来添加编译选项。这里我们使用了一种特殊的语法：`$<$<COMPILE_LANGUAGE:C>:...>`。这是 CMake 的**生成器表达式**。

生成器表达式的格式是 `$<条件:真值>`。如果条件为真，整个表达式就展开为"真值"；如果条件为假，就展开为空字符串。`COMPILE_LANGUAGE:C` 是一个条件，表示"当前正在编译的文件是 C 语言"。所以 `$<$<COMPILE_LANGUAGE:C>:-Wall>` 的意思是："只对 C 语言文件应用 `-Wall` 选项"。

为什么需要这个？因为我们的项目同时有 ASM 和 C 文件。`-Wall`、`-Wextra` 这些选项是 GCC 的选项，对 NASM 无效。如果不加这个判断，NASM 会收到一堆不认识的选项然后报错。生成器表达式可以让我们精准地控制哪些选项应用到哪些语言的文件上。

这些选项的含义我来解释一下：`-Wall` 开启所有常用警告，`-Wextra` 开启额外警告，`-Werror` 把警告当错误（有警告就编译失败），`-std=c23` 使用 C23 标准，`-m64` 生成 64 位代码，`-mno-sse` 和 `-mno-sse2` 禁用 SSE 和 SSE2 指令集。最后一个可能让你困惑，为什么禁用 SSE？因为在内核早期启动阶段，SSE 相关的寄存器可能还没有初始化，使用 SSE 指令可能会导致崩溃。我们手动控制何时启用 SSE。

### 第五步：Debug/Release 特定标志

不同的构建类型需要不同的编译器标志，我们这样设置：

```cmake
# Debug/Release 特定标志
if(NOT CMAKE_C_FLAGS_DEBUG)
    set(CMAKE_C_FLAGS_DEBUG "-g -O0")
endif()
if(NOT CMAKE_C_FLAGS_RELEASE)
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()
```

`CMAKE_C_FLAGS_DEBUG` 和 `CMAKE_C_FLAGS_RELEASE` 是 CMake 的内置变量，分别用于 Debug 和 Release 构建的 C 编译器标志。这里的逻辑和前面的 `CMAKE_BUILD_TYPE` 类似：如果用户没有设置这些变量，我们就设置默认值。

Debug 模式下，`-g` 表示生成调试信息，`-O0` 表示不优化。不优化可以让调试更容易，因为代码的执行顺序和源代码一致。Release 模式下，`-O3` 表示最高级别的优化，`-DNDEBUG` 定义 `NDEBUG` 宏，这会让很多 assert 检查失效。

---

## 添加子目录

CMake 的强大之处在于模块化。我们把项目分成几个子目录，每个子目录负责自己的构建规则。

```cmake
# 子目录
add_subdirectory(boot)
add_subdirectory(kernel)
```

`add_subdirectory` 告诉 CMake 去处理指定目录下的 `CMakeLists.txt`。每个子目录都有自己的 `CMakeLists.txt`，定义自己的构建规则。这样我们的项目结构就是：

```
.
├── CMakeLists.txt        # 根配置
├── boot/
│   ├── bootloader.asm
│   └── CMakeLists.txt    # Bootloader 子项目
├── kernel/
│   ├── kernel_entry.asm
│   ├── kernel_main.c
│   └── CMakeLists.txt    # 内核子项目
└── cmake/
    └── ccos_config.h.in  # 配置头文件模板
```

这种结构的好处是每个模块的构建规则是独立的，互不干扰。如果你以后想加一个新模块，比如 `drivers`，只需要创建 `drivers/` 目录，在里面写一个 `CMakeLists.txt`，然后在根 `CMakeLists.txt` 里加一行 `add_subdirectory(drivers)` 就行了。

---

## 第一次构建测试

先别急，我们来验证一下 CMake 配置是否正确。在写完整配置之前，我们先创建一个能跑的最小版本，不要一次写太多。万一有问题，排查起来会非常困难。

### 创建最小化的 CMakeLists.txt

我们在项目根目录创建 `CMakeLists.txt`，内容如下：

```cmake
cmake_minimum_required(VERSION 3.20)
project(CCOS_x64 ASM C)

# 工具链检测
find_program(NASM nasm REQUIRED)
find_program(QEMU qemu-system-x86_64 REQUIRED)
find_program(PYTHON python3 REQUIRED)

message(STATUS "Found NASM: ${NASM}")
message(STATUS "Found QEMU: ${QEMU}")
message(STATUS "Found Python: ${PYTHON}")

# 构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
```

保存这个文件，然后我们试着运行 CMake 配置。

### 运行 CMake 配置

打开终端，在项目根目录下运行：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

`-B build` 表示构建目录是 `build`，CMake 会在那里生成构建文件。`-DCMAKE_BUILD_TYPE=Debug` 设置构建类型为 Debug。你应该看到类似这样的输出：

```
-- Found NASM: /usr/bin/nasm
-- Found QEMU: /usr/bin/qemu-system-x86_64
-- Found Python: /usr/bin/python3
-- Build type: Debug
-- Configuring done
-- Generating done
-- Build files have been written to: /path/to/build
```

如果你看到这些信息，恭喜你，CMake 配置成功了！如果看到错误，别慌，仔细看错误信息。通常是工具没安装或者路径不对。

### ⚠️ 常见坑点

这里有几个常见的坑点，我先说一下，免得你踩进去。

如果 CMake 报错说找不到工具，检查一下工具是否真的安装了。用 `which nasm` 查看命令是否存在，用 `nasm -v` 查看版本。如果提示命令不存在，说明没安装或者 PATH 不对。WSL 用户特别注意：Windows 的 PATH 和 Linux 的 PATH 是分开的，你在 Windows 里装了工具，WSL 里不一定能用。

如果 CMake 版本太低，会提示 "CMake 3.20 or higher is required"。你需要升级 CMake。在 Ubuntu 上，可以用 `sudo apt install cmake` 安装最新版。如果版本还是不够，可能需要从官网下载或者用 PPA。

还有一个容易被忽视的问题：文件权限。如果你的用户对当前目录没有写权限，CMake 可能无法创建 `build` 目录。用 `ls -la` 查看权限，必要时用 `chmod` 修改。

---

## CMake 缓存问题

这里有一个很重要的问题，很多人都会踩坑。CMake 会把配置结果缓存在 `build/CMakeCache.txt` 里。这个缓存是为了加速配置过程，避免每次都重新检测所有东西。但这也意味着，如果你改了配置，直接改参数可能不生效。

举个实际的例子。你第一次配置时用了 `cmake -B build -DCMAKE_BUILD_TYPE=Debug`，然后你想切换到 Release，运行了 `cmake -B build -DCMAKE_BUILD_TYPE=Release`。但你发现构建类型还是 Debug，为什么？因为缓存里的值覆盖了你新指定的值。

正确的做法是：删除缓存重新配置。

```bash
# 删除整个 build 目录
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

或者只删除缓存文件：

```bash
# 只删除缓存文件
rm build/CMakeCache.txt
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

这一点真的非常关键，很多人就是因为忘记删缓存，配置改了半天不生效，最后崩溃。

---

## 子目录的 CMakeLists.txt

现在我们来写子目录的 `CMakeLists.txt`。首先是 `boot/CMakeLists.txt`，这个比较简单，因为我们暂时只编译一个汇编文件。

```cmake
# Bootloader 构建
add_custom_command(
    OUTPUT bootloader.bin
    COMMAND ${NASM} -f bin
        bootloader.asm
        -o bootloader.bin
    DEPENDS bootloader.asm
    COMMENT "Building bootloader"
)
```

`add_custom_command` 是 CMake 的命令，用来添加自定义的构建命令。`OUTPUT` 指定输出文件，`COMMAND` 指定要执行的命令，`DEPENDS` 指定依赖文件，`COMMENT` 是构建时显示的注释信息。

这里我们用 NASM 编译 `bootloader.asm`，输出纯二进制格式的 `bootloader.bin`。`-f bin` 表示输出纯二进制，这是 bootloader 需要的格式。

然后是 `kernel/CMakeLists.txt`，这个稍微复杂一点，因为我们有汇编入口和 C 代码。

```cmake
# 内核汇编入口
add_custom_command(
    OUTPUT kernel_entry.o
    COMMAND ${NASM} -f elf64
        kernel_entry.asm
        -o kernel_entry.o
    DEPENDS kernel_entry.asm
    COMMENT "Building kernel entry"
)

# 内核 C 代码（暂时留空，下一篇再讲）
```

这里我们用 NASM 编译 `kernel_entry.asm`，但这次输出的是 ELF64 格式的目标文件（`.o`）。`-f elf64` 表示生成 64 位 ELF 格式，这是链接器需要的格式。C 代码部分我们留到下一篇再讲，因为涉及到 freestanding 环境的配置，需要单独解释。

---

## 生成器表达式再深入

让我们再深入理解一下生成器表达式。这是 CMake 非常强大的特性，理解了它，你就能写出更灵活的配置。

### 基本语法

生成器表达式的格式是：

```
$<条件:真值>
```

如果条件为真，就展开为"真值"；否则展开为空字符串。这听起来很简单，但它的用途非常广泛。

### 常用条件

CMake 提供了很多内置条件，我们来列举一些常用的：

```cmake
# 只对 C 语言应用
$<COMPILE_LANGUAGE:C>

# 只对 C++ 语言应用
$<COMPILE_LANGUAGE:CXX>

# 只在 Debug 构建时应用
$<CONFIG:Debug>

# 只在 64 位平台上应用
$<SIZEOF_POINTER:8>
```

这些条件可以组合使用，比如 `$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:Debug>>` 表示"只在 Debug 模式下编译 C 文件时应用"。

### 实际例子

让我们看几个实际的例子：

```cmake
# 只在 Debug 模式下添加调试符号
add_compile_options(
    $<$<CONFIG:Debug>:-g>
)

# 只对 C++ 语言应用 C++ 标准标志
add_compile_options(
    $<COMPILE_LANGUAGE:CXX>:-std=c++23>
)

# 只对 64 位平台应用特定选项
add_compile_options(
    $<$<SIZEOF_POINTER:8>:-m64>
)
```

这些例子展示了生成器表达式的灵活性。你可以根据语言、构建类型、平台等条件，精准地控制编译选项。

---

## 项目结构验证

现在我们来验证一下项目结构是否正确。用 `tree` 命令查看（如果没有安装，可以用 `sudo apt install tree`）：

```bash
tree -L 2
```

你应该看到类似这样的输出：

```
.
├── CMakeLists.txt
├── boot
│   ├── bootloader.asm
│   └── CMakeLists.txt
├── build
│   ├── CMakeCache.txt
│   └── ...
├── cmake
│   └── ccos_config.h.in
└── kernel
    ├── CMakeLists.txt
    ├── kernel_entry.asm
    └── kernel_main.c
```

如果结构看起来不对，检查一下是否漏了某个文件或目录。确保每个子目录都有 `CMakeLists.txt`，即使内容很简单。

---

## 下一步

很好，到这里我们已经了解了 CMake 的基本结构。我们学会了：
- 如何声明 CMake 版本和项目
- 如何检测工具链
- 如何设置编译器标志
- 如何使用生成器表达式
- 如何配置 Debug/Release 构建
- 如何添加子目录

但事情到这里还没完。我们只是搭好了架子，**还没真正编译任何东西**。在下一篇里，我们会创建实际的内核代码——先是汇编入口，然后是 C 主函数。你会发现这两者是如何配合工作的，以及为什么需要这种两阶段的设计。

在继续之前，建议你先把上面的配置都试一遍，确保 CMake 能正常配置。如果你遇到任何问题，先停下来解决它。一个坚实的起点，比一堆带问题的配置要好得多。

---

<div align="center">

## 文档导航

[← 为什么要迁移到CMake](01_为什么要迁移到CMake.md)  | [创建内核C代码入口 →](03_创建内核C代码入口.md)

</div>
