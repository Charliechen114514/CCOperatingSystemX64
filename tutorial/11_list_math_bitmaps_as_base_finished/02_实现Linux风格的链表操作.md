# 实现 Linux 风格的链表操作 —— Stage 11 链表实战指南（续）

## 前言

在上一篇文章中，我们建立了侵入式链表的基础框架，理解了 `list_head` 结构体和 `container_of` 宏的原理。但光有这些还不够，一个真正可用的链表还需要一系列操作函数：插入、删除、遍历、查询。这些操作看起来简单，但实现起来有不少细节需要注意。

说实话，我刚接触链表的时候觉得这玩意儿太简单了，不就是几个指针操作吗？但真正动手实现的时候才发现，事情没那么简单。插入节点时要考虑头插和尾插，删除节点时要防止遍历崩溃，遍历链表时要处理删除当前节点的情况。更别提还有链表拼接、切割这些高级操作。

所以这一篇我们会一起实现完整的链表操作 API。从最基础的插入删除开始，逐步构建出 Linux 风格的链表操作集合。每写一个函数我都会解释清楚它的实现原理和使用场景，确保你不仅知道怎么用，还知道为什么这么设计。准备好了吗？我们继续。

---

## 环境说明

在开始之前，确保你已经完成了上一篇文章的内容，`kernel/list/list.h` 已经创建并且基础测试已经通过。

```
前置条件：
  - kernel/list/list.h 已定义 list_head 结构体
  - LIST_HEAD 和 INIT_LIST_HEAD 已实现
  - list_entry 宏已通过测试
  - 日志系统正常工作
```

这一篇我们会创建 `kernel/list/list.c` 文件，实现具体的链表操作函数。这些函数有些会在 `list.h` 中声明为内联函数，有些会放在 `.c` 文件中实现。区分的标准很简单：能用内联函数实现的就尽量内联，这样性能更好；复杂的逻辑或者不能内联的函数就放到 `.c` 文件中。

---

## 第一步：理解双向链表的插入操作

在开始写代码之前，我们先理解一下双向链表的插入操作是怎么工作的。双向链表的一个优势是插入操作非常高效，只需要修改几个指针，不需要遍历整个链表。

假设我们有两个节点 `prev` 和 `next`，它们在链表中是相邻的。现在我们要在它们之间插入一个新节点 `new_node`：

```
插入前：
prev <--> next

插入后：
prev <--> new_node <--> next
```

需要做的是：
1. 让 `next->prev` 指向 `new_node`
2. 让 `new_node->next` 指向 `next`
3. 让 `new_node->prev` 指向 `prev`
4. 让 `prev->next` 指向 `new_node`

这个操作的顺序其实是有讲究的。我们先把新节点的指针设置好，然后再修改原有节点的指针。这样可以避免在操作过程中链表暂时处于不一致状态。

理解了这个原理，我们就开始实现吧。

---

## 第二步：实现内部插入函数

现在我们来创建 `kernel/list/list.c` 文件，实现最基础的插入函数。

```c
/**
 * @file list.c
 * @brief Linux kernel style intrusive doubly-linked list implementation.
 * @date 2026-02-17
 */
#include "list.h"

/* ===== 内部辅助函数 ===== */

/**
 * @brief 在两个节点之间插入新节点（内部函数）
 *
 * 这是所有插入操作的底层实现，其他插入函数都基于此
 *
 * @param new_node 要插入的新节点
 * @param prev 前驱节点
 * @param next 后继节点
 */
void __list_add(list_head* new_node, list_head* prev, list_head* next) {
    next->prev = new_node;
    new_node->next = next;
    new_node->prev = prev;
    prev->next = new_node;
}
```

这个函数是所有插入操作的基础。注意这个函数声明时没有 `static`，因为它需要在 `list.h` 中被其他函数调用。但它的名字以双下划线开头，表示这是一个内部函数，用户不应该直接调用。

等等，你可能会问：为什么要有一个专门的内部函数，而不是直接在 `list_add` 和 `list_add_tail` 里实现这些逻辑？问得好。这样设计是因为 `list_add` 和 `list_add_tail` 可以声明为 `static inline` 函数放在头文件里，这样编译器可以内联它们，提高性能。而具体的指针操作逻辑放在 `list.c` 中，避免代码重复。

现在我们需要在 `list.h` 中声明这个函数。在 `list.h` 中添加：

```c
/* ===== 内部辅助函数（在 list.c 中实现）===== */

void __list_add(list_head* new_node, list_head* prev, list_head* next);
void __list_del(list_head* prev, list_head* next);
```

---

## 第三步：实现用户调用的插入函数

现在我们来实现用户实际调用的插入函数：`list_add`（头插法）和 `list_add_tail`（尾插法）。

在 `list.h` 中添加：

```c
/* ===== 基础链表操作 ===== */

/**
 * @brief 在链表头部插入节点
 *
 * 新节点会被插入到 head 之后，也就是链表的第一个位置
 *
 * @param new_node 要插入的节点
 * @param head 链表头
 */
static inline void list_add(list_head* new_node, list_head* head) {
    __list_add(new_node, head, head->next);
}

/**
 * @brief 在链表尾部插入节点
 *
 * 新节点会被插入到 head 之前，也就是链表的最后一个位置
 *
 * @param new_node 要插入的节点
 * @param head 链表头
 */
static inline void list_add_tail(list_head* new_node, list_head* head) {
    __list_add(new_node, head->prev, head);
}
```

这两个函数都是内联函数，非常简单，就是调用 `__list_add` 时传入不同的参数。

`list_add` 在 `head` 和 `head->next` 之间插入新节点，也就是链表的头部。如果你连续调用 `list_add`，后插入的节点会在前面，结果是逆序的。

`list_add_tail` 在 `head->prev` 和 `head` 之间插入新节点，也就是链表的尾部。如果你连续调用 `list_add_tail`，插入的顺序和遍历顺序一致。

让我用一个例子说明。假设我们要维护一个任务队列，新来的任务应该排在队列末尾，那就应该用 `list_add_tail`。如果要实现一个栈结构，后进先出，那就应该用 `list_add`。

---

## 第四步：实现删除操作

插入之后就是删除。双向链表的删除操作同样很简单，只需要把要删除节点的前驱和后继直接连起来就行了。

在 `list.c` 中添加：

```c
/**
 * @brief 删除两个节点之间的节点（内部函数）
 *
 * 这是所有删除操作的底层实现
 *
 * @param prev 前驱节点
 * @param next 后继节点
 */
void __list_del(list_head* prev, list_head* next) {
    next->prev = prev;
    prev->next = next;
}
```

然后在 `list.h` 中添加用户调用的删除函数：

```c
/**
 * @brief 从链表中删除节点
 *
 * 节点被从链表中移除，但指针不会被清空
 * 如果需要重新使用这个节点，应该使用 list_del_init
 *
 * @param entry 要删除的节点
 */
static inline void list_del(list_head* entry) {
    __list_del(entry->prev, entry->next);
}

/**
 * @brief 从链表中删除节点并重新初始化
 *
 * 节点被从链表中移除后，指针会被重置为指向自己
 * 这样节点就变成了一个独立的空链表，可以重新使用
 *
 * @param entry 要删除的节点
 */
void list_del_init(list_head* entry);
```

`list_del` 的实现非常简单，就是调用 `__list_del`。注意删除后节点的 `next` 和 `prev` 指针并没有被清空，它们仍然指向原来的位置。这其实是有意为之的，因为在某些情况下我们需要保留这些信息。但如果你要重新使用这个节点，就必须重新初始化它。

`list_del_init` 在删除节点后会调用 `INIT_LIST_HEAD` 重置指针，这样节点就变成一个独立的空链表，可以安全地重新使用。

我们在 `list.c` 中实现 `list_del_init`：

```c
void list_del_init(list_head* entry) {
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}
```

---

## 第五步：实现遍历宏

链表的插入和删除都有了，现在我们需要遍历链表。遍历链表最直观的方式就是用 `for` 循环，从 `head->next` 开始，一直走到 `head` 为止。

在 `list.h` 中添加：

```c
/* ===== 链表遍历宏 ===== */

/**
 * @brief 遍历链表
 *
 * @param pos 用于循环的 list_head 指针
 * @param head 要遍历的链表头
 */
#define list_for_each(pos, head) \
    for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

/**
 * @brief 反向遍历链表
 *
 * @param pos 用于循环的 list_head 指针
 * @param head 要遍历的链表头
 */
#define list_for_each_prev(pos, head) \
    for ((pos) = (head)->prev; (pos) != (head); (pos) = (pos)->prev)
```

这两个宏非常直观。`list_for_each` 从前往后遍历，`list_for_each_prev` 从后往前遍历。

但这里有个坑：如果你在遍历过程中删除了当前节点，`pos->next` 就会被修改，循环就会出问题。我们来做一个"安全"版本的遍历宏，它会提前保存下一个节点的指针：

```c
/**
 * @brief 遍历链表，支持删除当前节点
 *
 * 这个变种会在每次迭代前保存下一个节点，允许安全删除当前节点
 *
 * @param pos 用于循环的 list_head 指针
 * @param n 另一个 list_head 指针，用于存储下一个节点
 * @param head 要遍历的链表头
 */
#define list_for_each_safe(pos, n, head)                       \
    for ((pos) = (head)->next, (n) = (pos)->next; (pos) != (head); \
         (pos) = (n), (n) = (pos)->next)
```

`list_for_each_safe` 多了一个参数 `n`，用来在每次循环开始时保存下一个节点。这样即使在循环体中删除了 `pos`，我们仍然可以通过 `n` 找到下一个节点。

让我举个例子说明为什么需要这个"安全"版本。假设你要遍历链表并删除某些满足条件的节点：

```c
list_head* pos, *n;
list_for_each_safe(pos, n, &task_list) {
    task_t* task = list_entry(pos, task_t, list);
    if (should_delete_task(task)) {
        list_del(pos);  // 安全删除
        free(task);
    }
}
```

如果用普通的 `list_for_each`，删除 `pos` 后，`pos->next` 已经被修改了，下次循环就会出错。而 `list_for_each_safe` 预先保存了 `n`，所以删除 `pos` 也不会影响循环继续进行。

---

## 第六步：实现类型安全的遍历宏

到目前为止，我们的遍历宏都是遍历 `list_head` 指针的。但在实际使用中，我们更常见的需求是遍历包含 `list_head` 的结构体。每次都要调用 `list_entry` 宏有点麻烦，我们可以做一个更方便的宏。

在 `list.h` 中添加：

```c
/**
 * @brief 获取链表中第一个结构体
 *
 * @param ptr 链表头
 * @param type 包含结构体的类型
 * @param member list_head 在结构体中的成员名
 * @return 指向第一个结构体的指针，如果链表为空则返回 NULL
 */
#define list_first_entry(ptr, type, member) \
    (list_is_empty(ptr) ? NULL : list_entry((ptr)->next, type, member))

/**
 * @brief 获取下一个结构体
 *
 * @param pos 当前结构体指针
 * @param member list_head 在结构体中的成员名
 * @return 下一个结构体的指针
 */
#define list_next_entry(pos, member) \
    list_entry((pos)->member.next, __typeof__(*(pos)), member)

/**
 * @brief 安全地获取下一个结构体
 *
 * 与 list_next_entry 不同，这个宏会检查下一个节点是否是链表头
 *
 * @param pos 当前结构体指针
 * @param head 链表头
 * @param member list_head 在结构体中的成员名
 * @return 下一个结构体的指针，如果已到链表末尾则返回 NULL
 */
#define list_next_entry_safe(pos, head, member) \
    ((pos)->member.next == (head) ? NULL : list_next_entry(pos, member))
```

现在我们可以实现类型安全的遍历宏了：

```c
/**
 * @brief 遍历给定类型的链表
 *
 * @param pos 用于循环的类型指针
 * @param head 要遍历的链表头
 * @param member list_head 在结构体中的成员名
 */
#define list_for_each_entry(pos, head, member)                 \
    for ((pos) = list_first_entry(head, __typeof__(*(pos)), member); \
         (pos) != NULL; (pos) = list_next_entry_safe(pos, head, member))
```

这个宏看起来有点复杂，我们慢慢拆解。首先用 `list_first_entry` 获取第一个结构体，然后每次循环用 `list_next_entry_safe` 获取下一个。当返回 `NULL` 时，说明已经遍历完整个链表了。

使用起来非常方便：

```c
task_t* pos;
list_for_each_entry(pos, &task_list, list) {
    klog_info("Task PID: %d\n", pos->pid);
}
```

这样写比用 `list_for_each` 加 `list_entry` 要简洁得多，而且类型更安全，编译器可以进行类型检查。

---

## 第七步：实现辅助查询函数

现在我们来实现一些辅助查询函数，比如检查链表是否为空、统计链表长度等。

在 `list.h` 中添加：

```c
/**
 * @brief 检查链表是否为空
 *
 * @param head 要检查的链表头
 * @return true 如果链表为空，false 否则
 */
static inline bool list_is_empty(const list_head* head) {
    return head->next == head;
}

/**
 * @brief 检查节点是否是链表的最后一个节点
 *
 * @param entry 要检查的节点
 * @param head 链表头
 * @return true 如果节点是最后一个，false 否则
 */
static inline bool list_is_last(const list_head* entry, const list_head* head) {
    return entry->next == head;
}

/**
 * @brief 检查链表是否只有一个节点
 *
 * @param head 要检查的链表头
 * @return true 如果链表只有一个节点，false 否则
 */
static inline bool list_is_singular(const list_head* head) {
    return !list_is_empty(head) && (head->next == head->prev);
}
```

这些函数都很简单，但使用频率很高。`list_is_empty` 检查链表是否为空，`list_is_last` 检查节点是否是最后一个，`list_is_singular` 检查链表是否只有一个节点。

我们在 `list.c` 中再实现一个统计链表长度的函数：

```c
/**
 * @brief 统计链表中的节点数量
 *
 * @param head 链表头
 * @return 链表中的节点数量
 */
size_t list_count(const list_head* head) {
    size_t count = 0;
    list_head* pos;

    list_for_each(pos, head) {
        count++;
    }

    return count;
}
```

这个函数的时间复杂度是 O(n)，因为需要遍历整个链表。如果你需要频繁获取链表长度，最好自己维护一个计数变量。

---

## 第八步：更新 CMakeLists.txt

现在我们有了 `list.c` 文件，需要确保 CMakeLists.txt 正确配置。

检查 `kernel/list/CMakeLists.txt`：

```cmake
# Linked list implementation
add_library(list STATIC
    list.c
)

target_include_directories(list PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kernel
)
```

确保 `kernel/CMakeLists.txt` 包含这个子目录：

```cmake
add_subdirectory(list)
```

然后确保链接 `list` 库。如果你的内核主程序在 `kernel_main.c`，需要在对应的 CMakeLists.txt 中添加对 `list` 库的依赖。

---

## 第九步：编写完整测试验证功能

现在我们来测试一下链表操作是否正确工作。我们会创建一个任务结构体，然后用链表来管理这些任务。

在 `kernel_main.c` 中添加测试代码：

```c
#include "kernel/list/list.h"
#include "klogs/kprintf.h"

// 定义一个任务结构体
typedef struct task {
    int pid;
    char name[32];
    list_head list;  // 侵入式链表节点
} task_t;

// 全局任务队列
LIST_HEAD(task_queue);

void test_list_operations(void) {
    klog_info("=== 链表操作测试 ===\n");

    // 创建几个任务
    task_t task1 = { .pid = 1, .list = LIST_HEAD_INIT(task1.list) };
    strcpy(task1.name, "init");

    task_t task2 = { .pid = 2, .list = LIST_HEAD_INIT(task2.list) };
    strcpy(task2.name, "kthreadd");

    task_t task3 = { .pid = 3, .list = LIST_HEAD_INIT(task3.list) };
    strcpy(task3.name, "systemd");

    // 测试 list_add_tail（顺序插入）
    klog_info("\n测试 list_add_tail（顺序插入）：\n");
    list_add_tail(&task1.list, &task_queue);
    list_add_tail(&task2.list, &task_queue);
    list_add_tail(&task3.list, &task_queue);

    // 遍历输出
    task_t* pos;
    list_for_each_entry(pos, &task_queue, list) {
        klog_info("  Task: PID=%d, Name=%s\n", pos->pid, pos->name);
    }

    // 测试 list_count
    klog_info("\n链表长度: %d（预期：3）\n", list_count(&task_queue));

    // 测试 list_add（头插，逆序）
    klog_info("\n清空队列，测试 list_add（逆序插入）：\n");

    // 先清空队列
    list_head *p, *n;
    list_for_each_safe(p, n, &task_queue) {
        list_del(p);
    }

    // 头插法插入
    list_add(&task1.list, &task_queue);
    list_add(&task2.list, &task_queue);
    list_add(&task3.list, &task_queue);

    // 遍历输出
    list_for_each_entry(pos, &task_queue, list) {
        klog_info("  Task: PID=%d, Name=%s\n", pos->pid, pos->name);
    }
    klog_info("（注意：顺序是 3, 2, 1，因为是头插法）\n");

    // 测试删除操作
    klog_info("\n测试删除操作（删除 PID=2 的任务）：\n");
    list_for_each_entry_safe(pos, n, &task_queue, list) {
        if (pos->pid == 2) {
            klog_info("  删除任务: PID=%d, Name=%s\n", pos->pid, pos->name);
            list_del(&pos->list);
        }
    }

    klog_info("\n删除后的链表：\n");
    list_for_each_entry(pos, &task_queue, list) {
        klog_info("  Task: PID=%d, Name=%s\n", pos->pid, pos->name);
    }

    klog_info("\n所有测试完成！\n");
}
```

在 `kernel_main` 函数中调用这个测试：

```c
void kernel_main(void) {
    // ... 其他初始化代码 ...

    klog_init(KLOG_BACKEND_SERIAL);
    test_list_operations();

    // ... 其他代码 ...
}
```

编译并运行：

```bash
cd build
cmake ..
make
./run.sh
```

你应该在串口输出中看到类似这样的结果：

```
[INFO ] === 链表操作测试 ===
[INFO ]
[INFO ] 测试 list_add_tail（顺序插入）：
[INFO ]   Task: PID=1, Name=init
[INFO ]   Task: PID=2, Name=kthreadd
[INFO ]   Task: PID=3, Name=systemd
[INFO ]
[INFO ] 链表长度: 3（预期：3）
[INFO ]
[INFO ] 清空队列，测试 list_add（逆序插入）：
[INFO ]   Task: PID=3, Name=systemd
[INFO ]   Task: PID=2, Name=kthreadd
[INFO ]   Task: PID=1, Name=init
[INFO ] （注意：顺序是 3, 2, 1，因为是头插法）
[INFO ]
[INFO ] 测试删除操作（删除 PID=2 的任务）：
[INFO ]   删除任务: PID=2, Name=kthreadd
[INFO ]
[INFO ] 删除后的链表：
[INFO ]   Task: PID=3, Name=systemd
[INFO ]   Task: PID=1, Name=init
[INFO ]
[INFO ] 所有测试完成！
```

如果输出不对，先检查编译是否通过，然后看看头文件是否正确包含。我当时遇到的问题是忘记在 `kernel/CMakeLists.txt` 中链接 `list` 库，导致链接器报找不到符号的错误。确保你的内核主程序正确链接了 `list` 库。

---

## 到这里我们完成了什么

让我们回顾一下这篇文章的内容。我们实现了完整的链表操作 API，包括：

- `__list_add` — 内部插入函数
- `list_add` / `list_add_tail` — 头插和尾插
- `__list_del` — 内部删除函数
- `list_del` / `list_del_init` — 删除节点
- `list_for_each` / `list_for_each_safe` — 遍历链表
- `list_for_each_entry` — 类型安全的遍历
- `list_is_empty` / `list_is_last` / `list_is_singular` — 查询函数
- `list_count` — 统计节点数量

这些函数构成了一个功能完整的双向链表库。而且由于采用了侵入式设计，这个链表库可以用于任何类型的数据结构，只需要在结构体中嵌入一个 `list_head` 字段就行。

说实话，实现这个链表库的时候我踩了不少坑。最麻烦的是遍历宏的设计，尤其是支持删除的 `safe` 版本。如果 `n` 指针的获取顺序不对，很容易在链表只有一个节点的时候出 bug。还有就是 `list_for_each_entry` 宏中的 `__typeof__` 扩展，这是 GCC 的特性，如果换其他编译器可能需要调整。

---

## 接下来

在下一篇文章中，我们会：

1. 创建位操作数学库
2. 实现高效的 2 的幂次检测
3. 实现内存对齐操作
4. 理解位操作在内核开发中的重要性

准备好了吗？我们继续。

---

<div align="center">

## 文档导航

[← 为什么需要侵入式链表](01_为什么需要侵入式链表.md)  | [位操作与内存对齐的艺术 →](03_位操作与内存对齐的艺术.md)

</div>
