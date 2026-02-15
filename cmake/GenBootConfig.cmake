# ============================================================================
# 生成 Bootloader 配置文件
# ============================================================================
# 输入变量: BOOT_STAGE1, KERNEL_BIN, CONFIG_OUT
# ============================================================================

# 获取 bootloader stage1 文件大小
if(EXISTS "${BOOT_STAGE1}")
    file(READ ${BOOT_STAGE1} CONTENT HEX)
    string(LENGTH ${CONTENT} FILE_SIZE)

    # 计算占用的扇区数（向上取整）
    math(EXPR BOOT_SECTORS "(${FILE_SIZE} + 511) / 512")

    # 内核起始扇区 = bootloader 占用的扇区数
    math(EXPR KERNEL_LBA "${BOOT_SECTORS}")

    message(STATUS "Bootloader size: ${FILE_SIZE} bytes = ${BOOT_SECTORS} sectors")
    message(STATUS "Kernel LBA start: ${KERNEL_LBA}")
else()
    message(FATAL_ERROR "Bootloader stage1 binary not found: ${BOOT_STAGE1}")
endif()

# 获取 kernel.bin 文件大小
if(EXISTS "${KERNEL_BIN}")
    file(READ ${KERNEL_BIN} CONTENT HEX)
    string(LENGTH ${CONTENT} KERNEL_FILE_SIZE)

    # 计算内核占用的扇区数（向上取整）
    math(EXPR KERNEL_SECTORS "(${KERNEL_FILE_SIZE} + 511) / 512")

    message(STATUS "Kernel size: ${KERNEL_FILE_SIZE} bytes = ${KERNEL_SECTORS} sectors")
else()
    message(FATAL_ERROR "Kernel binary not found: ${KERNEL_BIN}")
endif()

# 计算 CHS 参数 (标准值: SPT=63, HPC=16)
set(SPT 63)
set(HPC 16)
math(EXPR SECTORS_PER_CYLINDER "${SPT} * ${HPC}")
math(EXPR KERNEL_CHS_C "${KERNEL_LBA} / ${SECTORS_PER_CYLINDER}")
math(EXPR REMAINDER "${KERNEL_LBA} % ${SECTORS_PER_CYLINDER}")
math(EXPR KERNEL_CHS_H "${REMAINDER} / ${SPT}")
math(EXPR KERNEL_CHS_S "(${REMAINDER} % ${SPT}) + 1")

message(STATUS "Kernel CHS: C=${KERNEL_CHS_C} H=${KERNEL_CHS_H} S=${KERNEL_CHS_S}")

# 生成汇编配置文件
file(WRITE ${CONFIG_OUT} "; Auto-generated boot config\n")
file(APPEND ${CONFIG_OUT} "%define KERNEL_LBA ${KERNEL_LBA}\n")
file(APPEND ${CONFIG_OUT} "%define KERNEL_SECTORS ${KERNEL_SECTORS}\n")
file(APPEND ${CONFIG_OUT} "%define KERNEL_CHS_C ${KERNEL_CHS_C}\n")
file(APPEND ${CONFIG_OUT} "%define KERNEL_CHS_H ${KERNEL_CHS_H}\n")
file(APPEND ${CONFIG_OUT} "%define KERNEL_CHS_S ${KERNEL_CHS_S}\n")

message(STATUS "Generated: ${CONFIG_OUT}")
