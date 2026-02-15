# CCOS GDB 初始化配置文件
# 使用方法: gdb -x .gdbinit kernel.bin

target remote :1234
set architecture i386:x86-64:intel
set print pretty on
set print array on
set print elements 0
# 使用 Intel 语法显示汇编（更易读）
set disassembly-flavor intel

# ==================== 调试模式开关 ====================
# 取消下面那行的注释，就会在 kernel_main 自动停下来
# break kernel_main
# echo \n已在 kernel_main 设置断点\n
# =====================================================

echo "Load Symbol file...\n"
symbol-file build/kernel.elf

# 常用调试命令
# ================
# c/continue      - 继续执行
# si/stepi        - 单步执行（汇编指令级）
# ni/nexti        - 单步执行（不进入函数调用）
# s/step          - 单步执行（源代码级）
# n/next          - 单步执行（不进入函数）
# info registers  - 显示寄存器状态
# info registers rip - 显示特定寄存器
# x/10i $pc       - 显示当前指令及后续 9 条
# x/10x 0x address - 以十六进制显示内存
# backtrace/bt    - 显示调用栈
# info breakpoints - 显示所有断点
# delete breakpoints n - 删除断点 n
# print variable   - 打印变量值
# disassemble      - 反汇编当前函数

display/i $pc

# 欢迎信息
echo \n
echo =======================================\n
echo CCOS Kernel GDB 调试环境已就绪\n
echo =======================================\n
echo 输入 'c' 开始执行，或 'si' 单步调试\n
echo 输入 'help' 查看更多命令\n
echo =======================================\n
echo \n
