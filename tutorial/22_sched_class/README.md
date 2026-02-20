# Stage 22 调度类框架教程

本教程介绍如何在 CCOS 中实现模块化的调度类框架，借鉴 Linux 的调度器架构，实现 Round-Robin 和 Priority 两种调度策略的共存与扩展。

## 教程目录

1. [从简单轮转到调度类框架](./01_从简单轮转到调度类框架.md)
   - 为什么 Stage 21 的简单调度不够用
   - 单一调度算法的局限性
   - Linux 风格的调度类架构
   - 本阶段要实现的功能概览

2. [调度类的基础设计](./02_调度类的基础设计.md)
   - 虚函数表模式的核心概念
   - sched_class_t 结构详解
   - 调度实体 (sched_entity) 的作用
   - Per-Policy 运行队列设计

3. [搭建调度类框架脚手架](./03_搭建调度类框架脚手架.md)
   - 创建 sched.h/sched.c 文件
   - 定义核心数据结构
   - 实现框架初始化函数
   - 调度类注册机制

4. [实现 Round-Robin 调度类](./04_实现Round-Robin调度类.md)
   - 为什么先实现 RR 调度
   - 队列管理函数实现
   - 任务选择逻辑
   - 时间片处理机制

5. [时间片与定时器集成](./05_时间片与定时器集成.md)
   - 1000Hz 时钟中断回调
   - task_tick 的实现细节
   - need_resched 重调度机制
   - 时间片重置的常见陷阱

6. [实现 Priority 调度类](./06_实现Priority调度类.md)
   - 优先级调度的应用场景
   - Active/Expired 队列机制
   - 优先级抢占判断
   - 动态时间片分配

7. [进程管理与调度器集成](./07_进程管理与调度器集成.md)
   - PCB 中添加 sched_entity
   - fork 时设置调度策略
   - schedule() 函数的流程
   - 多策略任务选择逻辑

8. [编写演示程序](./08_编写演示程序.md)
   - RR 调度单元测试
   - Priority 调度验证
   - 抢占行为测试
   - 多策略共存演示

9. [常见问题与调试技巧](./09_常见问题与调试技巧.md)
   - 类注册失败的排查
   - 任务不被调用的原因
   - class_data 内存分配问题
   - 调试日志和 GDB 技巧

## 阅读建议

- 按顺序阅读，每篇文章依赖前一篇的内容
- 动手实践：每篇文章都有对应的代码实现
- 理解"为什么"：设计决策背后的原因
- 参考源码：代码示例对应 `kernel/process/` 下的实际文件

## 你将学到什么

完成本教程后，你将掌握：

- 模块化调度器的设计思想
- 虚函数表在 C 语言中的应用
- 多种调度算法的共存机制
- 时间片管理与定时器集成
- 优先级抢占的实现方法
- 可扩展架构的设计原则

## 相关资源

### 项目文档
- [../../PROGRESS.md](../../PROGRESS.md) - 项目进度
- [../../document/22_sched_class/](../../document/22_sched_class/) - 文档中心
- [../20_syscallframework/](../20_syscallframework/) - 上一阶段教程

### 源码文件
- [../../kernel/process/sched.h](../../kernel/process/sched.h) - 调度类框架接口
- [../../kernel/process/sched.c](../../kernel/process/sched.c) - 调度类框架实现
- [../../kernel/process/sched_rr.h](../../kernel/process/sched_rr.h) - RR 调度类接口
- [../../kernel/process/sched_rr.c](../../kernel/process/sched_rr.c) - RR 调度类实现
- [../../kernel/process/sched_prio.h](../../kernel/process/sched_prio.h) - Priority 调度类接口
- [../../kernel/process/sched_prio.c](../../kernel/process/sched_prio.c) - Priority 调度类实现
- [../../kernel/demo/sched/sched_demo.h](../../kernel/demo/sched/sched_demo.h) - 演示程序接口

### 外部参考
- [Linux Scheduling](https://www.kernel.org/doc/html/latest/scheduler/) - Linux 调度器文档
- [OSDev Scheduling](https://wiki.osdev.org/Scheduling_Algorithms) - OSDev 调度算法指南
- [OSTEP Scheduling](https://github.com/ostep/ostep-projects/blob/master/scheduling-intro) - OSTEP 调度章节

## 版本信息

- **阶段**: Stage 22
- **提交**: `9375e0b sched class`
- **日期**: 2026-02-20
- **作者**: CharlieChen

---

**作者**: CharlieChen114514
**最后更新**: 2026-02-20
