# CMake 集成与验证

我们来到了这个阶段的最后一篇文章。

之前我们实现了：
- 完整的字符串工具库
- 断言系统
- VGA 错误显示
- 主机环境测试框架

现在我们要把这些都集成到 CMake 构建系统中，让开发流程变得顺畅。

---

## 我们要达成的目标

集成完成后，应该能这样使用：

```bash
# 构建
cmake --build build

# 运行主机测试（快速验证）
cmake --build build --target test_string
./build/test/test_string

# 运行内核（集成测试）
cmake --build build --target run
```

---

## 第一步：查看当前的 CMake 结构

首先，让我们看看现有的 CMake 文件：

```bash
# 查看项目根目录的 CMakeLists.txt
cat CMakeLists.txt

# 查看 kernel 目录的 CMakeLists.txt
cat kernel/CMakeLists.txt

# 检查是否有 test 目录的 CMakeLists.txt
ls test/CMakeLists.txt 2>/dev/null || echo "Not found yet"
```

---

## 第二步：更新 kernel/CMakeLists.txt

我们需要把字符串库和断言系统添加到内核构建中。

```cmake
# kernel/CMakeLists.txt 的修改

# 添加 base 子目录
add_subdirectory(base)

# 添加 assert 子目录
add_subdirectory(assert)

# ... 现有的 driver/, welcomes/ 等保持不变 ...
```

**创建 `kernel/base/CMakeLists.txt`**：

```cmake
# kernel/base/CMakeLists.txt

# 收集源文件
set(BASE_SOURCES
    string.c
)

# 创建静态库
add_library(kernel_base STATIC ${BASE_SOURCES})

# 设置包含目录
target_include_directories(kernel_base
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/kernel/defines
)

# 设置编译选项
target_compile_options(kernel_base
    PRIVATE
        -Wall
        -Wextra
        -ffreestanding
)

# 禁用 libstdc++
target_link_options(kernel_base
    INTERFACE
        -nodefaultlibs
)
```

**创建 `kernel/assert/CMakeLists.txt`**：

```cmake
# kernel/assert/CMakeLists.txt

# 收集源文件
set(ASSERT_SOURCES
    assert.c
    assert_action_backend.c
)

# 创建静态库
add_library(kernel_assert STATIC ${ASSERT_SOURCES})

# 设置包含目录
target_include_directories(kernel_assert
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/kernel/defines
        ${CMAKE_SOURCE_DIR}/kernel/driver/vga
)

# 设置编译选项
target_compile_options(kernel_assert
    PRIVATE
        -Wall
        -Wextra
        -ffreestanding
)

# 依赖 VGA 驱动
target_link_libraries(kernel_assert
    PUBLIC
        kernel_vga
)
```

---

## 第三步：创建 test/CMakeLists.txt

```cmake
# test/CMakeLists.txt

# 主机环境测试程序
add_executable(test_string
    test_string.c
)

# 包含内核的 string.c
target_sources(test_string
    PRIVATE
        ../kernel/base/string.c
)

# 设置包含目录
target_include_directories(test_string
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/kernel/base
)

# 添加测试目标（需要 CTest 支持）
enable_testing()
add_test(NAME string_test COMMAND test_string)

# 自定义目标：方便运行测试
add_custom_target(run_string_test
    COMMAND test_string
    DEPENDS test_string
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test
    COMMENT "Running string library tests..."
)
```

---

## 第四步：处理链接问题

⚠️ **重要：内核链接 vs 主机链接**

内核代码是 `freestanding` 的，主机测试是 `hosted` 的。我们需要确保它们使用不同的链接选项。

**主机测试不需要特殊链接**：

```cmake
# test/CMakeLists.txt

# 主机测试程序使用标准链接
# 不需要 -ffreestanding 或 -nodefaultlibs
```

**内核代码需要 freestanding 链接**：

```cmake
# kernel/CMakeLists.txt

# 内核主程序使用 freestanding 链接
target_compile_options(kernel_main
    PRIVATE
        -ffreestanding
)

target_link_options(kernel_main
    PRIVATE
        -nostdlib
        -nodefaultlibs
)
```

---

## 第五步：修改内核主程序使用新库

更新 `kernel/kernel_main.c` 或相关文件，使用新的字符串库：

```c
/**
 * @file kernel_main.c
 * @brief Kernel main entry point.
 */

#include "base/string.h"
#include "assert/assert.h"
#include "driver/vga/vga.h"

void kernel_main(void)
{
    // 初始化 VGA
    vga_init();

    // 测试字符串函数
    const char* hello = "Hello, CCOS!";
    size_t len = strlen(hello);

    // 打印到 VGA
    vga_puts("CCOS Kernel v0.1.0\n");
    vga_puts(hello);
    vga_puts("\n");

    // 测试断言（暂时注释掉）
    // CCOS_ASSERT(1 == 2);  // 这会触发断言失败

    // 主循环
    while (1) {
        __asm__ volatile ("hlt");
    }
}
```

更新 `kernel/CMakeLists.txt` 链接新库：

```cmake
# 链接内核主程序
target_link_libraries(kernel_main
    PRIVATE
        kernel_base
        kernel_assert
        kernel_vga
        # ... 其他依赖 ...
)
```

---

## 第六步：完整构建流程

现在让我们构建并测试：

```bash
# 配置构建目录
cmake -DCMAKE_BUILD_TYPE=Debug -B build

# 构建所有目标
cmake --build build

# 运行主机测试
./build/test/test_string
# 或者
cmake --build build --target run_string_test

# 运行内核
cmake --build build --target run
```

---

## 第七步：添加便捷脚本

创建一些脚本让日常开发更方便：

**`scripts/test.sh`**：

```bash
#!/bin/bash
# 快速运行主机测试

set -e

echo "Building tests..."
cmake --build build --target test_string

echo ""
echo "Running tests..."
./build/test/test_string

echo ""
echo "✅ All tests passed!"
```

**`scripts/run_kernel.sh`**：

```bash
#!/bin/bash
# 快速运行内核

set -e

echo "Building kernel..."
cmake --build build

echo ""
echo "Starting kernel..."
cmake --build build --target run
```

**`scripts/full_test.sh`**：

```bash
#!/bin/bash
# 完整测试流程：主机测试 + 内核测试

set -e

echo "=========================================="
echo "Step 1: Running host tests..."
echo "=========================================="
./scripts/test.sh

echo ""
echo "=========================================="
echo "Step 2: Running kernel..."
echo "=========================================="
./scripts/run_kernel.sh

echo ""
echo "✅ All tests passed!"
```

给脚本添加执行权限：

```bash
chmod +x scripts/test.sh
chmod +x scripts/run_kernel.sh
chmod +x scripts/full_test.sh
```

---

## 第八步：验证集成

让我们验证整个集成是否正常工作：

### 8.1 主机测试验证

```bash
./scripts/test.sh
```

**预期输出**：

```
========================================
Step 1: Running host tests...
========================================
Building tests...
[构建输出...]

Running tests...
╔════════════════════════════════════════════════════════════╗
║           CCOS String Library Test Suite                  ║
╚════════════════════════════════════════════════════════════╝

--- Testing strlen...
  [PASS] strlen: all tests passed
...

========================================
Test Results:
  Passed: 30
  Failed: 0
========================================

✅ All tests passed!
```

### 8.2 内核运行验证

```bash
./scripts/run_kernel.sh
```

**预期输出**：

QEMU 窗口应该显示：
```
CCOS Kernel v0.1.0
Hello, CCOS!
```

串口输出也应该有相同的日志。

### 8.3 断言验证（可选）

暂时启用一个会失败的断言，验证 VGA 错误显示：

```c
// 在 kernel_main.c 中临时添加
CCOS_ASSERT(1 == 2);  // 这会失败
```

重新构建运行，你应该看到：
1. 屏幕变红
2. 显示断言失败信息
3. 系统停止

测试完后记得删除这行！

---

## 第九步：添加 Git 提交

代码验证通过后，提交到版本控制：

```bash
git add kernel/base/ kernel/assert/ test/
git add kernel/CMakeLists.txt test/CMakeLists.txt

git commit -m "feat: add string utility library and assertion system

- Implement complete C-standard string functions:
  - Basic: strlen, strnlen, strcpy, strncpy
  - Comparison: strcmp, strncmp, strcasecmp, strncasecmp
  - Search: strchr, strrchr, strstr, strpbrk, strspn, strcspn
  - Tokenization: strtok, strtok_r

- Add assertion system with VGA error display:
  - CCOS_ASSERT (always enabled)
  - CCOS_DEBUG_ASSERT (debug only)
  - White-on-red error screen with file/line/function info

- Add host-environment test framework:
  - Fast unit tests without QEMU overhead
  - Test macros: TEST_ASSERT_EQ, TEST_ASSERT_STR_EQ, etc.
  - Comprehensive test coverage for all functions

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>"
```

---

## 第十步：检查清单

在完成这个阶段之前，请确认：

- [ ] 创建了 `kernel/base/CMakeLists.txt`
- [ ] 创建了 `kernel/assert/CMakeLists.txt`
- [ ] 创建了 `test/CMakeLists.txt`
- [ ] 更新了 `kernel/CMakeLists.txt`
- [ ] 主机测试能够编译和运行
- [ ] 内核能够正常启动
- [ ] 字符串函数在内核中正常工作
- [ ] 断言系统能够正确报告错误
- [ ] 创建了便捷的测试脚本
- [ ] 代码已提交到 Git

---

## 总结

恭喜你！我们完成了 stage 08 的所有内容：

### 实现的功能

1. **字符串工具库**
   - 20+ 个标准 C 字符串函数
   - 完全兼容 freestanding 环境
   - 全面的单元测试覆盖

2. **断言系统**
   - `CCOS_ASSERT` 始终启用
   - `CCOS_DEBUG_ASSERT` 仅 Debug 模式
   - VGA 白字红底错误显示
   - GDB 断点集成

3. **测试框架**
   - 主机环境快速验证
   - 独立测试程序
   - 清晰的测试输出

### 开发流程优化

- 主机测试：几秒钟验证逻辑
- 内核测试：验证集成
- 一键脚本：简化日常开发

### 下一阶段预览

有了这些基础设施，下一阶段我们可以：
- 实现内存管理
- 实现文件系统
- 添加更多内核功能

一切就绪，让我们继续前进！

---

## 附录：完整的文件结构

```
CCOperatingSystemX64/
├── kernel/
│   ├── base/
│   │   ├── CMakeLists.txt
│   │   ├── string.h
│   │   └── string.c
│   ├── assert/
│   │   ├── CMakeLists.txt
│   │   ├── assert.h
│   │   ├── assert.c
│   │   ├── assert_action_backend.h
│   │   └── assert_action_backend.c
│   └── CMakeLists.txt (updated)
├── test/
│   ├── CMakeLists.txt
│   ├── test_string.c
│   └── host_support.h
├── scripts/
│   ├── test.sh (new)
│   ├── run_kernel.sh (new)
│   └── full_test.sh (new)
└── tutorial/
    └── 08_string_utils/
        ├── 01_为什么需要自实现字符串库.md
        ├── 02_从零实现字符串操作函数.md
        ├── 03_实现字符串比较函数.md
        ├── 04_实现字符串搜索函数.md
        ├── 05_实现字符串分割函数.md
        ├── 06_实现断言系统.md
        ├── 07_VGA错误显示后端.md
        ├── 08_主机环境测试框架.md
        └── 09_CMake集成与验证.md
```

---

**恭喜完成 stage 08！🎉**


---

<div align="center">

## 文档导航

[← 主机环境测试框架](08_主机环境测试框架.md)  | [README →](../09_memory_serial/README.md)

</div>
