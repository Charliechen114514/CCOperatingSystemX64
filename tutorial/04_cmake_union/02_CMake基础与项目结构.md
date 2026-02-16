# 02 - CMake 基础与项目结构

现在我们开始从零创建 CMake 构建系统。

---

## 从 0 开始创建 CMakeLists.txt

我们在项目根目录创建 `CMakeLists.txt`。先别急，我们一行一行来解释。

### 第一步：声明 CMake 版本和项目

```cmake
cmake_minimum_required(VERSION 3.20)
project(CCOS_x64 ASM C)
```

**为什么需要这两行？**

- `cmake_minimum_required` — 告诉 CMake 我们需要至少 3.20 版本。这样如果有人用老版本 CMake，会立即得到清晰的错误提示
- `project` — 定义项目名称和支持的语言。注意这里我们声明了 `ASM` 和 `C` 两种语言，CMake 会自动检测对应的编译器

### 第二步：工具链检测

现在我们来做 Makefile 做不到的事情 — 在构建前检查工具是否存在：

```cmake
find_program(NASM nasm REQUIRED)
find_program(QEMU qemu-system-x86_64 REQUIRED)
find_program(PYTHON python3 REQUIRED)

message(STATUS "Found NASM: ${NASM}")
message(STATUS "Found QEMU: ${QEMU}")
message(STATUS "Found Python: ${PYTHON}")
```

**`find_program` 做了什么？**

它在系统 PATH 中查找指定的程序。`REQUIRED` 表示如果找不到，CMake 配置阶段就会失败并报错，不会等到构建中途才崩溃。

**`message(STATUS ...)` 是什么？**

这是 CMake 的输出语句。`STATUS` 表示这是一条普通信息（不是警告或错误），在配置时会显示出来。

实际运行时你会看到：

```
-- Found NASM: /usr/bin/nasm
-- Found QEMU: /usr/bin/qemu-system-x86_64
-- Found Python: /usr/bin/python3
```

### 第三步：设置构建类型

CMake 内置了几种构建配置：

```cmake
# 设置构建类型（支持 Debug/Release）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
```

**这是什么意思？**

- `CMAKE_BUILD_TYPE` 是 CMake 的内置变量
- 如果用户没有指定（`NOT CMAKE_BUILD_TYPE`），默认设为 `Release`
- `CACHE STRING` 表示这个值会被缓存，下次运行 cmake 时记住用户的设置

你可以这样切换构建类型：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

### 第四步：编译器标志

现在我们设置 C 编译器的通用选项：

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

**这里有个很重要的语法：`$<$<COMPILE_LANGUAGE:C>:...>`**

这是 CMake 的**生成器表达式**。它的意思是："**只对 C 语言文件应用这些标志**"。

为什么需要这个？因为我们的项目同时有 ASM 和 C 文件，而 `-Wall` 这些选项只对 C 编译器有效。如果不加这个判断，NASM 会收到一堆不认识的选项然后报错。

**各个选项的含义**：

| 选项 | 含义 |
|------|------|
| `-Wall` | 开启所有常用警告 |
| `-Wextra` | 开启额外警告 |
| `-Werror` | 把警告当错误（警告就编译失败） |
| `-std=c23` | 使用 C23 标准 |
| `-m64` | 生成 64 位代码 |
| `-mno-sse` | 禁用 SSE 指令集 |
| `-mno-sse2` | 禁用 SSE2 指令集 |

### 第五步：Debug/Release 特定标志

```cmake
# Debug/Release 特定标志
if(NOT CMAKE_C_FLAGS_DEBUG)
    set(CMAKE_C_FLAGS_DEBUG "-g -O0")
endif()
if(NOT CMAKE_C_FLAGS_RELEASE)
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()
```

**这些标志的区别**：

| 配置 | 标志 | 含义 |
|------|------|------|
| Debug | `-g -O0` | 生成调试信息，无优化 |
| Release | `-O3 -DNDEBUG` | 最高优化，禁用断言 |

---

## 添加子目录

CMake 的强大之处在于模块化。我们把项目分成几个子目录：

```cmake
# 子目录
add_subdirectory(boot)
add_subdirectory(kernel)
```

每个子目录都有自己的 `CMakeLists.txt`，负责自己的构建规则。

**项目结构**：

```
.
├── CMakeLists.txt        # 根配置
├── boot/
│   └── CMakeLists.txt    # Bootloader 子项目
├── kernel/
│   └── CMakeLists.txt    # 内核子项目
└── cmake/
    └── ccos_config.h.in  # 配置头文件模板
```

---

## 第一次构建测试

先别急，我们来验证一下 CMake 配置是否正确。

### 创建最小化的 CMakeLists.txt

我们先创建一个能跑的最小版本，不要一次写太多：

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

把这个保存到 `CMakeLists.txt`。

### 运行 CMake 配置

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

**你会看到类似这样的输出**：

```
-- Found NASM: /usr/bin/nasm
-- Found QEMU: /usr/bin/qemu-system-x86_64
-- Found Python: /usr/bin/python3
-- Build type: Debug
-- Configuring done
-- Generating done
-- Build files have been written to: /path/to/build
```

### ⚠️ 注意

**如果 CMake 报错说找不到工具，检查一下：**

1. 工具是否真的安装了（`which nasm`）
2. PATH 环境变量是否正确（`echo $PATH`）
3. WSL 用户注意：Windows 路径和 Linux 路径是分开的

---

## CMake 缓存问题

CMake 会把配置结果缓存在 `build/CMakeCache.txt` 里。如果你想重新配置，**直接改参数可能不生效**。

**正确的做法**：

```bash
# 删除缓存重新配置
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

或者：

```bash
# 只删除缓存文件
rm build/CMakeCache.txt
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

---

## 子目录的 CMakeLists.txt

我们来看看 `kernel/CMakeLists.txt` 应该怎么写。

### boot/CMakeLists.txt（简单版）

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

### kernel/CMakeLists.txt（简单版）

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

---

## 生成器表达式再深入

让我们再深入理解一下生成器表达式。这是 CMake 非常强大的特性。

### 基本语法

```
$<条件:真值>
```

如果条件为真，就展开为"真值"，否则展开为空字符串。

### 常用条件

```cmake
# 只对 C 语言应用
$<COMPILE_LANGUAGE:C>

# 只在 Debug 构建时应用
$<CONFIG:Debug>

# 只在 64 位平台上应用
$<SIZEOF_POINTER:8>
```

### 实际例子

```cmake
# 只在 Debug 模式下添加调试符号
add_compile_options(
    $<$<CONFIG:Debug>:-g>
)

# 只对 C++ 语言应用 C++ 标准标志
add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++23>
)
```

---

## 下一步

很好，到这里我们已经了解了：

1. CMake 的基本结构
2. 工具链检测
3. 编译器标志设置
4. 生成器表达式的使用
5. Debug/Release 配置

但事情到这里还没完。我们只是搭好了架子，**还没真正编译任何东西**。

下一篇我们会创建实际的内核代码 — 先是汇编入口，然后是 C 主函数。你会发现这两者是如何配合工作的。

---

**上一篇**：[01 - 为什么要迁移到 CMake](./01_为什么要迁移到CMake.md)
**下一篇**：[03 - 创建内核 C 代码入口](./03_创建内核C代码入口.md)


---

<div align="center">

## 文档导航

[← 为什么要迁移到CMake](01_为什么要迁移到CMake.md)  | [创建内核C代码入口 →](03_创建内核C代码入口.md)

</div>
