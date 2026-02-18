# BSS 段过大导致内核启动失败排查报告

## 问题描述

**症状**: Debug 模式下内核无法正常启动，无法进入 `kernel_start` 函数。而 main 分支下相同模式可以正常启动。

**分支对比**:
- `main` 分支: Debug 模式正常工作
- `develop` 分支: Debug 模式无法进入 `kernel_start`

**时间**: 2026-02-18

## 现象详情

### main 分支（正常）
- 行为: 正常进入 kernel_start 和 kernel_main
- BSS 段大小: 约 140KB（结束地址约 0x23485）

### develop 分支（异常）
- 行为: 无法进入 kernel_start，在 bootloader 阶段或切换到保护模式后崩溃
- BSS 段大小: 约 2.1MB (0x207298 字节)

## 排查过程

### 1. 差异对比

使用 `git diff` 对比两个分支的关键文件：

```bash
# 对比 bootloader 配置
git diff main...HEAD -- boot/bootloader.asm

# 对比 CMakeLists.txt
git diff main...HEAD -- kernel/CMakeLists.txt

# 对比内核模块
git diff main...HEAD -- kernel/mm/pframe/pframe.c
```

### 2. 符号表分析

检查当前分支的内核符号表，找出占用空间最大的符号：

```bash
nm build/kernel.elf --size-sort --radix d | tail -30
```

**关键发现**:

| 符号名 | 大小 | 类型 |
|--------|------|------|
| `s_frame_bitmap_storage` | 2097152 (2MB) | BSS |
| `s_syscall_table` | 6144 (6KB) | BSS |
| `idt` | 4096 (4KB) | BSS |
| `s_pid_bitmap_buffer` | 4096 (4KB) | BSS |
| `s_regions` | 4096 (4KB) | BSS |

### 3. BSS 段布局分析

检查 ELF 段布局：

```bash
objdump -h build/kernel.elf | grep -E ".text|.bss|.data"
```

**结果**:

```
Idx Name          Size      VMA               LMA
  0 .text         0001a0aa  0000000000010000  0000000000010000
  1 .rodata       00005428  000000000002a0c0  000000000002a0c0
  2 .data         00000418  000000000002f500  000000000002f500
  5 .bss          00207298  000000000002f940  000000000002f940
```

**内存布局计算**:
- `.text` 结束: `0x10000 + 0x1a0aa = 0x2a0aa`
- `.rodata` 结束: `0x2a0c0 + 0x5428 = 0x2f4e8`
- `.data` 结束: `0x2f500 + 0x418 = 0x2f918`
- `.bss` 开始: `0x2f940`
- `.bss` 结束: `0x2f940 + 0x207298 = 0x236BD8` (约 2.1MB 处)

### 4. 根本原因定位

**问题代码** (`kernel/mm/pframe/pframe.c`):

```c
// develop 分支
static byte_t s_frame_bitmap_storage[PFRAME_MAX_BITMAP_SIZE]
    __attribute__((section(".bss")));

// main 分支
static byte_t s_frame_bitmap_storage[PFRAME_MAX_BITMAP_SIZE];
```

其中 `PFRAME_MAX_BITMAP_SIZE = 2 * 1024 * 1024` (2MB)

**差异分析**:
- `main` 分支: 没有强制指定 section，链接器可能将其放入 `.lbss` (large BSS)
- `develop` 分支: 使用 `__attribute__((section(".bss")))` 强制放入 `.bss`

## 根本原因

1. **BSS 段溢出**: `s_frame_bitmap_storage` 占用 2MB，加上其他 BSS 符号，总大小超过 2MB

2. **内存映射问题**:
   - bootloader 映射了 4MB 内存 (0x00000000 - 0x003FFFFF)
   - 但 BSS 段的初始化或访问可能在页表完全建立前发生
   - Debug 模式可能有额外的零初始化操作

3. **链接器布局变化**: 强制指定 `.bss` section 改变了链接器的默认行为

## 解决方案

### 方案 1: 移除强制 section 属性（推荐）

将 `pframe.c` 中的声明恢复为与 main 分支一致：

```c
static byte_t s_frame_bitmap_storage[PFRAME_MAX_BITMAP_SIZE];
```

### 方案 2: 调整链接器脚本

在 `linker.ld` 中为大 BSS 对象创建独立的 section：

```ld
.bss : {
    __bss_start = .;
    *(.bss)
    *(.bss.*)
    *(COMMON)
    __bss_end = .;
}

.lbss (NOLOAD) : {
    __lbss_start = .;
    *(.lbss)
    *(.lbss.*)
    __lbss_end = .;
}
```

### 方案 3: 动态分配

将静态分配改为动态分配（在内核初始化后从堆中分配）。

## 经验总结

1. **谨慎使用 section 属性**: `__attribute__((section(...")))` 会改变链接器的默认行为，可能导致意外的内存布局

2. **BSS 段大小监控**: 在构建过程中添加 BSS 段大小检查：

   ```cmake
   # 检查 BSS 段大小
   execute_process(
       COMMAND ${CMAKE_OBJDUMP} -h ${CMAKE_BINARY_DIR}/kernel.elf
       COMMAND awk "/\.bss/ {print \\$3, \\$4}"
       OUTPUT_VARIABLE BSS_INFO
   )
   ```

3. **内存映射一致性**: 确保 bootloader 的页表映射覆盖内核所有段的实际使用范围

4. **分支对比的重要性**: 使用 `git diff` 快速定位关键差异

5. **工具链**:
   - `nm --size-sort`: 按大小排序显示符号
   - `objdump -h`: 显示段布局
   - `readelf -S`: 显示节区头

## 相关文件

- `kernel/mm/pframe/pframe.c`: 问题代码所在
- `boot/bootloader.asm`: 页表设置代码
- `linker.ld`: 链接器脚本
- `kernel/CMakeLists.txt`: 构建配置
