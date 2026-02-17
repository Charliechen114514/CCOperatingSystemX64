# CMake 集成与验证 —— 把所有东西整合到一起

我们来到了这个阶段的最后一篇文章。之前我们实现了完整的字符串工具库、断言系统、VGA 错误显示和主机环境测试框架。现在要把这些都集成到 CMake 构建系统中，让开发流程变得顺畅。

## 我们要达成的目标

集成完成后，构建和测试应该是一行命令的事情。构建用 `cmake --build build`，运行主机测试用 `./build/test/test_string`，运行内核用 `cmake --build build --target run`。简单、直接、高效。

## 第一步：更新 kernel/CMakeLists.txt

我们需要把字符串库和断言系统添加到内核构建中。这很简单，就是在 `kernel/CMakeLists.txt` 中添加两个子目录。

```cmake
# kernel/CMakeLists.txt 的修改

# 添加 base 子目录
add_subdirectory(base)

# 添加 assert 子目录
add_subdirectory(assert)
```

然后创建两个新的 CMakeLists.txt 文件。`kernel/base/CMakeLists.txt` 创建静态库 `kernel_base`，设置包含目录和编译选项。`kernel/assert/CMakeLists.txt` 创建静态库 `kernel_assert`，同样设置包含目录和编译选项，并链接 VGA 驱动。

这里有个细节要注意：内核代码是 freestanding 的，所以编译选项要加上 `-ffreestanding`，链接选项要加上 `-nodefaultlibs`。而主机测试不需要这些选项，因为它使用标准库。

## 第二步：创建 test/CMakeLists.txt

测试程序的 CMakeLists.txt 相对简单。我们创建一个可执行文件，包含我们的测试代码和字符串函数。设置包含目录让编译器能找到头文件。

关键是区分内核链接和主机链接。内核代码用 freestanding 链接，主机测试用标准链接。这个区别很重要，弄混了会导致链接错误。

## 第三步：修改内核主程序

现在内核主程序可以使用新的字符串库了。我们只需要包含头文件，然后就可以调用这些函数。比如 `strlen`、`strcmp` 等等。

```c
#include "base/string.h"
#include "assert/assert.h"
#include "driver/vga/vga.h"

void kernel_main(void) {
    vga_init();

    const char* hello = "Hello, CCOS!";
    size_t len = strlen(hello);

    vga_puts("CCOS Kernel v0.1.0\n");
    vga_puts(hello);
    vga_puts("\n");

    while (1) {
        __asm__ volatile ("hlt");
    }
}
```

更新 `kernel/CMakeLists.txt` 链接新库，这样编译的时候就能找到这些函数的实现。

## 第四步：验证集成

让我们验证整个集成是否正常工作。首先配置构建目录，然后编译所有目标，最后运行测试。

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -B build
cmake --build build
./build/test/test_string
cmake --build build --target run
```

如果一切正常，主机测试应该全部通过，内核应该能正常启动并在 VGA 上显示启动信息。

## 第五步：添加便捷脚本

为了简化日常开发，我们可以创建几个脚本。`scripts/test.sh` 快速运行主机测试，`scripts/run_kernel.sh` 快速运行内核，`scripts/full_test.sh` 运行完整的测试流程。

这些脚本很简单，就是几行命令的组合。但它们能节省很多时间，因为你不用每次都输入长长的命令。

## 总结

恭喜你，我们完成了 stage 08 的所有内容。我们实现了一个功能完整的字符串工具库，包括基础操作、比较、搜索、分割等 20 多个函数。我们实现了断言系统，包括始终启用的关键检查和仅 Debug 模式的调试辅助。我们实现了 VGA 错误显示，白字红底让断言失败一目了然。我们还实现了主机环境测试框架，让代码验证变得快速而简单。

最重要的是，我们建立了一套完整的开发流程：主机测试快速验证逻辑，内核测试验证集成，一键脚本简化日常开发。这套流程会让后续的开发变得顺畅很多。

有了这些基础设施，下一阶段我们可以实现内存管理、文件系统、更多内核功能。一切就绪，让我们继续前进！


---

<div align="center">

## 文档导航

[← 主机环境测试框架](08_主机环境测试框架.md)  | [README →](../09_memory_serial/README.md)

</div>
