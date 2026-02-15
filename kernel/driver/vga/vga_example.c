/**
 * @file vga_example.c
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief VGA驱动酷炫演示 - 展示VGA驱动的各项功能
 * @version 0.1
 * @date 2026-02-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "vga.h"
#include "vga_config.h"
#include "vga_helpers.h"

// VGA颜色定义 (0-15)
#define VGA_COLOR_BLACK 0
#define VGA_COLOR_BLUE 1
#define VGA_COLOR_GREEN 2
#define VGA_COLOR_CYAN 3
#define VGA_COLOR_RED 4
#define VGA_COLOR_MAGENTA 5
#define VGA_COLOR_BROWN 6
#define VGA_COLOR_LIGHT_GREY 7
#define VGA_COLOR_DARK_GREY 8
#define VGA_COLOR_BRIGHT_BLUE 9
#define VGA_COLOR_BRIGHT_GREEN 10
#define VGA_COLOR_BRIGHT_CYAN 11
#define VGA_COLOR_BRIGHT_RED 12
#define VGA_COLOR_BRIGHT_MAGENTA 13
#define VGA_COLOR_YELLOW 14
#define VGA_COLOR_WHITE 15

// 简单延时循环 (在内核中没有精确计时)
static void vga_delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm__ volatile("nop");
    }
}

// 在指定位置绘制单个字符（带颜色）
static void vga_put_char_at(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, char c, vga_color_t font,
                            vga_color_t bg) {
    if (vga == NULL || x >= vga->width || y >= vga->height)
        return;

    volatile uint16_t* video = (volatile uint16_t*)vga->base_addr;
    uint16_t entry = (uint16_t)c | ((uint16_t)(bg << 4 | font) << 8);
    video[y * vga->width + x] = entry;
}

// 绘制矩形边框
static void vga_draw_rect(CCOS_VGA* vga, vga_sz_t x, vga_sz_t y, vga_sz_t width, vga_sz_t height,
                          vga_color_t color) {
    // 绘制四个角
    vga_put_char_at(vga, x, y, '+', color, VGA_COLOR_BLACK);
    vga_put_char_at(vga, x + width - 1, y, '+', color, VGA_COLOR_BLACK);
    vga_put_char_at(vga, x, y + height - 1, '+', color, VGA_COLOR_BLACK);
    vga_put_char_at(vga, x + width - 1, y + height - 1, '+', color, VGA_COLOR_BLACK);

    // 绘制边框
    for (vga_sz_t i = 1; i < width - 1; i++) {
        vga_put_char_at(vga, x + i, y, '-', color, VGA_COLOR_BLACK);
        vga_put_char_at(vga, x + i, y + height - 1, '-', color, VGA_COLOR_BLACK);
    }
    for (vga_sz_t i = 1; i < height - 1; i++) {
        vga_put_char_at(vga, x, y + i, '|', color, VGA_COLOR_BLACK);
        vga_put_char_at(vga, x + width - 1, y + i, '|', color, VGA_COLOR_BLACK);
    }
}

// 显示彩虹颜色条
static void vga_rainbow_demo(CCOS_VGA* vga) {
    vga_color_t colors[] = {VGA_COLOR_BLUE,        VGA_COLOR_GREEN,       VGA_COLOR_CYAN,
                            VGA_COLOR_RED,         VGA_COLOR_MAGENTA,     VGA_COLOR_BROWN,
                            VGA_COLOR_LIGHT_GREY,  VGA_COLOR_BRIGHT_BLUE, VGA_COLOR_BRIGHT_GREEN,
                            VGA_COLOR_BRIGHT_CYAN, VGA_COLOR_BRIGHT_RED,  VGA_COLOR_BRIGHT_MAGENTA,
                            VGA_COLOR_YELLOW,      VGA_COLOR_WHITE};
    int num_colors = sizeof(colors) / sizeof(colors[0]);

    vga_sz_t bar_width = vga->width / num_colors;
    vga_sz_t bar_height = 8;

    for (int i = 0; i < num_colors; i++) {
        for (vga_sz_t y = 2; y < 2 + bar_height; y++) {
            for (vga_sz_t x = i * bar_width; x < (i + 1) * bar_width && x < vga->width; x++) {
                vga_put_char_at(vga, x, y, ' ', VGA_COLOR_BLACK, colors[i]);
            }
        }
    }

    // 标题
    vga_set_cursor(vga, vga->width / 2 - 6, 12);
    vga_color_t fc = VGA_COLOR_WHITE;
    set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
    vga_print_string(vga, "RAINBOW DEMO");
}

// ASCII艺术logo
static void vga_show_logo(CCOS_VGA* vga) {
    const char* logo[] = {"  _____  _____ _____    _____ ",    " /  _  \\/  ___/  __ \\  /  ___|",
                          " | | | |\\ `--.| /  \\/  \\ `--. ", " | | | | `--. \\ |      `--. \\",
                          " | |_| |/\\__/ / \\__/\\/\\__/ /",  "  \\___/\\____/ \\____/\\____/ "};

    vga_sz_t start_x = (vga->width - 28) / 2;
    vga_sz_t start_y = 16;

    vga_color_t fc = VGA_COLOR_BRIGHT_CYAN;
    set_vga_property(vga, &fc, CURSOR_FONT_COLOR);

    for (int i = 0; i < 6; i++) {
        vga_set_cursor(vga, start_x, start_y + i);
        vga_print_string(vga, logo[i]);
    }
}

// 颜色表格演示
static void vga_color_table_demo(CCOS_VGA* vga) {
    const char* color_names[] = {"BLACK", "BLUE",     "GREEN",  "CYAN",  "RED",    "MAGENTA",
                                 "BROWN", "LGREY",    "DGREY",  "BBLUE", "BGREEN", "BCYAN",
                                 "BRED",  "BMAGENTA", "YELLOW", "WHITE"};

    vga_sz_t start_x = 5;
    vga_sz_t start_y = 24;

    vga_color_t bg = VGA_COLOR_BLACK;
    set_vga_property(vga, &bg, CURSOR_BACKGROUND_COLOR);

    for (int i = 0; i < 16; i++) {
        vga_sz_t x = start_x + (i % 8) * 14;
        vga_sz_t y = start_y + (i / 8) * 2;

        // 绘制色块
        for (vga_sz_t j = 0; j < 12; j++) {
            vga_put_char_at(vga, x + j, y, ' ', (vga_color_t)i, (vga_color_t)i);
        }

        // 显示颜色名称
        vga_set_cursor(vga, x, y + 1);
        vga_color_t fc = VGA_COLOR_WHITE;
        set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
        vga_print_string(vga, color_names[i]);
    }
}

// 粒子效果演示（简单的星空中文字）
static void vga_starfield_demo(CCOS_VGA* vga) {
    // 清屏
    vga_clear(vga, VGA_COLOR_BLACK);

    const char stars[] = {'*', '.', '+', '\'', '^'};

    // 随机撒一些星星
    for (int i = 0; i < 500; i++) {
        vga_sz_t x = (vga_sz_t)(i * 73) % vga->width;
        vga_sz_t y = (vga_sz_t)(i * 137) % (vga->height - 10) + 5;
        char star = stars[(i / 100) % 5];
        vga_color_t color = (vga_color_t)(VGA_COLOR_WHITE - (i % 3));
        vga_put_char_at(vga, x, y, star, color, VGA_COLOR_BLACK);
    }

    // 添加标题
    vga_set_cursor(vga, vga->width / 2 - 5, 2);
    vga_color_t fc = VGA_COLOR_BRIGHT_CYAN;
    set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
    vga_print_string(vga, "*** STARS ***");

    vga_set_cursor(vga, vga->width / 2 - 10, vga->height - 3);
    vga_print_string(vga, "Welcome to CCOS VGA Demo!");
}

// 进度条动画
static void vga_progress_bar_demo(CCOS_VGA* vga) {
    vga_sz_t bar_x = 10;
    vga_sz_t bar_y = 35;
    vga_sz_t bar_width = 100;
    vga_sz_t bar_height = 3;

    // 绘制边框
    vga_draw_rect(vga, bar_x - 1, bar_y - 1, bar_width + 2, bar_height + 2, VGA_COLOR_WHITE);

    // 标题
    vga_set_cursor(vga, bar_x, bar_y - 3);
    vga_color_t fc = VGA_COLOR_YELLOW;
    set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
    vga_print_string(vga, "LOADING CCOS KERNEL...");

    // 动画填充
    for (vga_sz_t i = 0; i <= bar_width; i++) {
        vga_color_t progress_color;
        if (i < bar_width / 3) {
            progress_color = VGA_COLOR_RED;
        } else if (i < 2 * bar_width / 3) {
            progress_color = VGA_COLOR_YELLOW;
        } else {
            progress_color = VGA_COLOR_BRIGHT_GREEN;
        }

        for (vga_sz_t y = bar_y; y < bar_y + bar_height; y++) {
            vga_put_char_at(vga, bar_x + i, y, '#', progress_color, VGA_COLOR_BLACK);
        }

        // 简单延时
        vga_delay(100000);
    }

    // 完成消息
    vga_set_cursor(vga, bar_x, bar_y + bar_height + 2);
    fc = VGA_COLOR_BRIGHT_GREEN;
    set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
    vga_print_string(vga, "[OK] System Ready!");
}

// 文字滚动效果
static void vga_scrolling_text_demo(CCOS_VGA* vga) {
    const char* message = "    CCOS VGA DRIVER DEMO - Features: Color Display | Cursor Control | "
                          "Text Rendering | Graphics Primitives    ";

    vga_sz_t y = 42;
    vga_color_t bg = VGA_COLOR_BLUE;
    vga_clear(vga, bg);

    // 标题
    vga_set_cursor(vga, vga->width / 2 - 12, 5);
    vga_color_t fc = VGA_COLOR_YELLOW;
    set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
    vga_print_string(vga, "TEXT SCROLLING DEMO");

    int msg_len = 104; // 消息长度
    int offset = 0;

    for (int frame = 0; frame < 200; frame++) {
        // 清除滚动区域
        for (vga_sz_t x = 0; x < vga->width; x++) {
            vga_put_char_at(vga, x, y, ' ', VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        }

        // 绘制文字
        for (int i = 0; i < vga->width && (offset + i) < msg_len; i++) {
            if (offset + i >= 0) {
                vga_put_char_at(vga, i, y, message[offset + i], VGA_COLOR_WHITE, VGA_COLOR_BLUE);
            }
        }

        offset++;
        if (offset > msg_len)
            offset = -vga->width;

        vga_delay(50000);
    }
}

// 绘制系统信息面板
static void vga_system_info_demo(CCOS_VGA* vga) {
    vga_clear(vga, VGA_COLOR_BLACK);

    // 绘制多个信息框
    vga_draw_rect(vga, 2, 2, 35, 10, VGA_COLOR_BRIGHT_CYAN);
    vga_draw_rect(vga, 40, 2, 35, 10, VGA_COLOR_BRIGHT_GREEN);
    vga_draw_rect(vga, 78, 2, 35, 10, VGA_COLOR_BRIGHT_RED);

    vga_draw_rect(vga, 2, 15, 50, 8, VGA_COLOR_YELLOW);
    vga_draw_rect(vga, 55, 15, 58, 8, VGA_COLOR_MAGENTA);

    // 填充信息
    struct {
        const char* title;
        const char* lines[4];
        vga_color_t color;
    } panels[] = {
        {"VGA INFO",
         {"Resolution: 120x80", "Base Addr: 0xB8000", "Colors: 16", "Mode: Text 80x25"},
         VGA_COLOR_CYAN},
        {"KERNEL",
         {"Name: CCOS", "Version: 0.1.0", "Arch: x86_64", "Compiler: Clang"},
         VGA_COLOR_GREEN},
        {"MEMORY", {"Total: 16 MB", "Used: 2 MB", "Free: 14 MB", "Heap: OK"}, VGA_COLOR_RED},
        {"FEATURES",
         {"[+] VGA Text Mode", "[+] Color Support", "[+] Cursor Control", "[+] Scrolling"},
         VGA_COLOR_YELLOW},
        {"STATUS",
         {"[OK] VGA Driver", "[OK] Kernel Init", "[OK] Memory", "[OK] All Systems"},
         VGA_COLOR_MAGENTA}};

    vga_sz_t positions[][2] = {{4, 3}, {42, 3}, {80, 3}, {4, 16}, {57, 16}};

    for (int p = 0; p < 5; p++) {
        vga_set_cursor(vga, positions[p][0], positions[p][1]);
        vga_color_t fc = panels[p].color;
        set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
        vga_print_string(vga, panels[p].title);

        for (int i = 0; i < 4; i++) {
            vga_set_cursor(vga, positions[p][0], positions[p][1] + 2 + i);
            fc = VGA_COLOR_LIGHT_GREY;
            set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
            vga_print_string(vga, panels[p].lines[i]);
        }
    }

    // 底部状态栏
    for (vga_sz_t x = 0; x < vga->width; x++) {
        vga_put_char_at(vga, x, vga->height - 2, ' ', VGA_COLOR_BLACK, VGA_COLOR_DARK_GREY);
    }
    vga_set_cursor(vga, 2, vga->height - 2);
    vga_color_t fc = VGA_COLOR_WHITE;
    set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
    vga_print_string(vga, "CCOS VGA Driver Demo System - Press any key to continue...");

    vga_delay(5000000);
}

/**
 * @brief 主演示函数 - 展示VGA驱动的所有酷炫功能
 */
void vga_example_show() {
    // 获取VGA实例 (需要先调用 system_vga_init)
    CCOS_VGA* vga = (CCOS_VGA*)vga_instance();
    if (vga == NULL) {
        return;
    }

    // 1. 彩虹颜色演示
    vga_rainbow_demo(vga);
    vga_delay(3000000);

    // 2. 显示Logo
    vga_show_logo(vga);
    vga_delay(3000000);

    // 3. 颜色表格演示
    vga_clear(vga, VGA_COLOR_BLACK);
    vga_color_table_demo(vga);
    vga_delay(4000000);

    // 4. 星空粒子效果
    vga_starfield_demo(vga);
    vga_delay(4000000);

    // 5. 进度条动画
    vga_clear(vga, VGA_COLOR_BLACK);
    vga_progress_bar_demo(vga);
    vga_delay(3000000);

    // 6. 文字滚动效果
    vga_scrolling_text_demo(vga);

    // 7. 系统信息面板
    vga_system_info_demo(vga);

    // 最终清屏，返回干净状态
    vga_clear(vga, VGA_COLOR_BLACK);

    vga_set_cursor(vga, 0, 0);
    vga_color_t fc = VGA_COLOR_BRIGHT_GREEN;
    set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
    vga_print_string(vga, "VGA Demo Complete!\nSystem Ready.\n");
}
