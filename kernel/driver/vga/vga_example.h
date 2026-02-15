/**
 * @file vga_example.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief VGA驱动酷炫演示 - 头文件
 * @version 0.1
 * @date 2026-02-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "vga.h"

/**
 * @brief 主演示函数 - 展示VGA驱动的所有酷炫功能
 *
 * 演示内容包括：
 * - 彩虹颜色条展示 (16种VGA颜色)
 * - ASCII艺术Logo显示
 * - 颜色表格演示 (展示所有16种颜色)
 * - 星空粒子效果
 * - 进度条加载动画
 * - 文字滚动效果
 * - 系统信息多面板展示
 *
 * @note 使用前必须先调用 system_vga_init() 初始化VGA
 */
void vga_example_show();
