# Stage 20 系统调用框架教程

本教程介绍如何在 CCOS 中实现完整的 x86_64 系统调用框架，为用户空间程序提供与内核交互的标准接口。

## 教程目录

1. [从轮询到 syscall](./01_从轮询到syscall.md)
   - 为什么需要系统调用框架
   - 用户态与内核态的隔离
   - syscall/sysret 指令的优势

2. [syscall 指令的魔法](./02_syscall指令的魔法.md)
   - syscall 指令工作原理
   - sysretq 指令工作原理
   - MSR 寄存器详解
   - System V AMD64 ABI 调用约定

3. [搭建 syscall 脚手架](./03_搭建syscall脚手架.md)
   - 创建目录结构
   - 编写 CMakeLists.txt
   - 定义 syscall_numbers.h
   - 定义 syscall.h 数据结构

4. [MSR 寄存器配置实战](./04_MSR寄存器配置实战.md)
   - CPUID 特性检测
   - 启用 CR4 的 SCE 位
   - 配置 IA32_LSTAR、IA32_STAR、IA32_FMASK
   - 配置顺序的重要性

5. [汇编入口与栈对齐](./05_汇编入口与栈对齐.md)
   - syscall_handler 汇编入口实现
   - 寄存器保存/恢复
   - 栈对齐问题详解
   - 构造 syscall_frame_t

6. [int 0x80 向后兼容](./06_int0x80向后兼容.md)
   - 为什么需要 int 0x80 兼容
   - i386 系统调用约定
   - int0x80_handler 汇编入口实现
   - 参数格式转换

7. [C 语言分发器](./07_C语言分发器.md)
   - syscall_dispatch() 实现
   - 系统调用表设计
   - 边界检查与错误处理
   - 统计信息更新

8. [实现第一个系统调用](./08_实现第一个系统调用.md)
   - sys_write 实现
   - sys_getpid 实现
   - sys_exit 实现
   - 其他系统调用的占位实现

9. [统计系统与调试支持](./09_统计系统与调试支持.md)
   - 统计数据结构
   - syscall_dump_stats() 实现
   - 性能测量功能
   - 断点和跟踪支持

10. [编写用户程序测试 syscall](./10_编写用户程序测试syscall.md)
    - 用户态程序框架
    - 系统调用封装函数
    - 用户程序链接脚本
    - 内核态测试替代方案

## 阅读建议

- 按顺序阅读，每篇文章依赖前一篇的内容
- 动手实践：每篇文章都有代码示例
- 理解"为什么"：代码实现前会讲解原因
- 遇到问题：查看每篇文章末尾的"常见问题排查"章节

## 你将学到什么

完成本教程后，你将掌握：

- x86_64 系统调用机制的完整原理
- syscall/sysret 指令的使用方法
- MSR 寄存器的配置与使用
- 汇编入口代码的编写技巧
- 栈对齐问题的解决方法
- 系统调用分发器的设计与实现

## 相关资源

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目进度
- [../../document/20_syscallframework/](../../document/20_syscallframework/) - 文档中心
- [../19_tidy_codes_and_refactorize/](../19_tidy_codes_and_refactorize/) - 上一阶段教程

### 源码文件
- [../../kernel/syscall/syscall.h](../../kernel/syscall/syscall.h) - 系统调用框架接口
- [../../kernel/syscall/syscall.c](../../kernel/syscall/syscall.c) - 系统调用框架实现
- [../../kernel/syscall/syscall.asm](../../kernel/syscall/syscall.asm) - 汇编入口/出口
- [../../kernel/syscall/syscall_numbers.h](../../kernel/syscall/syscall_numbers.h) - 系统调用号定义
- [../../kernel/syscall/syscall_table.c](../../kernel/syscall/syscall_table.c) - 系统调用处理表

### 外部参考
- [x86_64 System Calls](https://wiki.osdev.org/System_Calls) - OSDev 系统调用指南
- [syscall Instruction](https://www.felixcloutier.com/x86/syscall) - Intel 指令参考
- [System V AMD64 ABI](https://gitlab.com/x86-psABIs/x86-64-ABI) - 调用约定规范
- [MSR Registers](https://wiki.osdev.org/Model-Specific_Registers) - MSR 寄存器详解

## 版本信息

- **阶段**: Stage 20
- **提交**: `4eee7ca syscall framework`
- **日期**: 2026-02-18
- **作者**: CharlieChen

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-20
