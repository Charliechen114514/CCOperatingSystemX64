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
#define CCOS_VGA_DEMO
#ifdef CCOS_VGA_DEMO
#    include "gui_helper/gui_helper.h"
#    include "vga.h"
#    include "vga_helpers.h"

// 显示彩虹颜色条
static void vga_rainbow_demo(CCOS_VGA* vga) {
    vga_color_t colors[] = {VGA_COLOR_BLUE,        VGA_COLOR_GREEN,       VGA_COLOR_CYAN,
                            VGA_COLOR_RED,         VGA_COLOR_MAGENTA,     VGA_COLOR_BROWN,
                            VGA_COLOR_LIGHT_GREY,  VGA_COLOR_BRIGHT_BLUE, VGA_COLOR_BRIGHT_GREEN,
                            VGA_COLOR_BRIGHT_CYAN, VGA_COLOR_BRIGHT_RED,  VGA_COLOR_BRIGHT_MAGENTA,
                            VGA_COLOR_YELLOW,      VGA_COLOR_WHITE};
    int num_colors = sizeof(colors) / sizeof(colors[0]);

    vga_sz_t bar_width = vga->width / num_colors;
    vga_sz_t bar_height = 3;
    vga_sz_t start_y = 2;

    for (int i = 0; i < num_colors; i++) {
        for (vga_sz_t y = start_y; y < start_y + bar_height && y < vga->height; y++) {
            for (vga_sz_t x = i * bar_width; x < (i + 1) * bar_width && x < vga->width; x++) {
                vga_put_char_at(vga, x, y, ' ', VGA_COLOR_BLACK, colors[i]);
            }
        }
    }

    // 标题
    vga_sz_t title_y = start_y + bar_height + 1;
    if (title_y < vga->height) {
        vga_set_cursor(vga, vga->width / 2 - 6, title_y);
        vga_color_t fc = VGA_COLOR_WHITE;
        set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
        vga_print_string(vga, "RAINBOW DEMO");
    }
}

// ASCII艺术logo
static void vga_show_logo(CCOS_VGA* vga) {
    const char* logo[] = {"  _____  _____ _____    _____ ",    " /  _  \\/  ___/  __ \\  /  ___|",
                          " | | | |\\ `--.| /  \\/  \\ `--. ", " | | | | `--. \\ |      `--. \\",
                          " | |_| |/\\__/ / \\__/\\/\\__/ /",  "  \\___/\\____/ \\____/\\____/ "};

    vga_sz_t start_x = (vga->width - 28) / 2;
    vga_sz_t start_y = 9; // 调整到屏幕中间区域

    vga_color_t fc = VGA_COLOR_BRIGHT_CYAN;
    set_vga_property(vga, &fc, CURSOR_FONT_COLOR);

    for (int i = 0; i < 6 && (start_y + i) < vga->height; i++) {
        vga_set_cursor(vga, start_x, start_y + i);
        vga_print_string(vga, logo[i]);
    }
}

// 颜色表格演示
static void vga_color_table_demo(CCOS_VGA* vga) {
    const char* color_names[] = {"BLACK", "BLUE",     "GREEN",  "CYAN",  "RED",    "MAGENTA",
                                 "BROWN", "LGREY",    "DGREY",  "BBLUE", "BGREEN", "BCYAN",
                                 "BRED",  "BMAGENTA", "YELLOW", "WHITE"};

    vga_sz_t start_x = 3;
    vga_sz_t start_y = 4; // 调整到安全区域

    vga_color_t bg = VGA_COLOR_BLACK;
    set_vga_property(vga, &bg, CURSOR_BACKGROUND_COLOR);

    for (int i = 0; i < 16; i++) {
        vga_sz_t col = i % 8;
        vga_sz_t row = i / 8;
        vga_sz_t x = start_x + col * 9;
        vga_sz_t y = start_y + row * 3;

        // 确保不超出屏幕
        if (y + 1 >= vga->height || x + 5 >= vga->width)
            continue;

        // 绘制色块
        for (vga_sz_t j = 0; j < 5; j++) {
            vga_put_char_at(vga, x + j, y, ' ', (vga_color_t)i, (vga_color_t)i);
        }

        // 显示颜色名称
        vga_set_cursor(vga, x, y + 1);
        vga_color_t fc = VGA_COLOR_WHITE;
        set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
        vga_print_string(vga, color_names[i]);
    }

    // 标题
    vga_set_cursor(vga, vga->width / 2 - 5, 1);
    vga_color_t fc = VGA_COLOR_YELLOW;
    set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
    vga_print_string(vga, "16 COLORS");
}

// 粒子效果演示（简单的星空中文字）
static void vga_starfield_demo(CCOS_VGA* vga) {
    // 清屏
    vga_clear(vga, VGA_COLOR_BLACK);

    const char stars[] = {'*', '.', '+', '\'', '^'};

    // 随机撒一些星星 - 限制在安全区域内
    for (int i = 0; i < 300; i++) {
        vga_sz_t x = (vga_sz_t)(i * 73) % vga->width;
        vga_sz_t y = (vga_sz_t)(i * 137) % (vga->height - 5) + 2;
        char star = stars[(i / 60) % 5];
        vga_color_t color = (vga_color_t)(VGA_COLOR_WHITE - (i % 3));
        vga_put_char_at(vga, x, y, star, color, VGA_COLOR_BLACK);
    }

    // 添加标题
    vga_set_cursor(vga, vga->width / 2 - 5, 1);
    vga_color_t fc = VGA_COLOR_BRIGHT_CYAN;
    set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
    vga_print_string(vga, "*** STARS ***");

    vga_set_cursor(vga, vga->width / 2 - 10, vga->height - 2);
    fc = VGA_COLOR_BRIGHT_GREEN;
    set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
    vga_print_string(vga, "Welcome to CCOS VGA Demo!");
}

// 进度条动画
static void vga_progress_bar_demo(CCOS_VGA* vga) {
    vga_sz_t bar_x = 5;
    vga_sz_t bar_y = 12; // 调整到屏幕中间
    vga_sz_t bar_width = 70;
    vga_sz_t bar_height = 2;

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

        for (vga_sz_t y = bar_y; y < bar_y + bar_height && y < vga->height; y++) {
            vga_put_char_at(vga, bar_x + i, y, '#', progress_color, VGA_COLOR_BLACK);
        }

        // 简单延时
        vga_delay(100000);
    }

    // 完成消息
    vga_sz_t msg_y = bar_y + bar_height + 2;
    if (msg_y < vga->height) {
        vga_set_cursor(vga, bar_x, msg_y);
        vga_color_t fc = VGA_COLOR_BRIGHT_GREEN;
        set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
        vga_print_string(vga, "[OK] System Ready!");
    }
}

// 文字滚动效果
static void vga_scrolling_text_demo(CCOS_VGA* vga) {
    const char* message = " CCOS VGA DRIVER DEMO - Features: Color Display | Text Rendering    ";

    vga_sz_t y = 18; // 调整到屏幕下半部分
    vga_color_t bg = VGA_COLOR_BLUE;
    vga_clear(vga, bg);

    // 标题
    vga_set_cursor(vga, vga->width / 2 - 12, 3);
    vga_color_t fc = VGA_COLOR_YELLOW;
    set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
    vga_print_string(vga, "TEXT SCROLLING DEMO");

    int msg_len = 70;
    int offset = 0;

    for (int frame = 0; frame < 150; frame++) {
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

        vga_delay(40000000);
    }
}

// 绘制系统信息面板
static void vga_system_info_demo(CCOS_VGA* vga) {
    vga_clear(vga, VGA_COLOR_BLACK);

    // 标题
    vga_set_cursor(vga, vga->width / 2 - 6, 1);
    vga_color_t fc = VGA_COLOR_BRIGHT_CYAN;
    set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
    vga_print_string(vga, "== CCOS INFO ==");

    // 使用 vga_panel_t 结构定义面板 - 调整为80x25屏幕
    vga_panel_t panels[] = {
        {2, 4, 36, 6, VGA_COLOR_BRIGHT_CYAN, VGA_COLOR_BLACK, VGA_COLOR_CYAN, "VGA INFO"},
        {42, 4, 36, 6, VGA_COLOR_BRIGHT_GREEN, VGA_COLOR_BLACK, VGA_COLOR_GREEN, "KERNEL"},
        {2, 12, 36, 6, VGA_COLOR_BRIGHT_RED, VGA_COLOR_BLACK, VGA_COLOR_RED, "MEMORY"},
        {42, 12, 36, 6, VGA_COLOR_YELLOW, VGA_COLOR_BLACK, VGA_COLOR_YELLOW, "FEATURES"}};

    const char* panel_contents[][4] = {{"Res: 80x25", "Base: 0xB8000", "Colors: 16", "Mode: Text"},
                                       {"Name: CCOS", "Ver: 0.1.0", "Arch: x86_64", "Clang: OK"},
                                       {"Total: 16MB", "Used: 2MB", "Free: 14MB", "Heap: OK"},
                                       {"[+] VGA Text", "[+] Colors", "[+] Cursor", "[+] Scroll"}};

    // 绘制所有面板
    for (int p = 0; p < 4; p++) {
        vga_draw_panel(vga, &panels[p]);

        // 填充面板内容
        for (int i = 0; i < 4; i++) {
            vga_set_cursor(vga, panels[p].x + 2, panels[p].y + 2 + i);
            vga_color_t fc = VGA_COLOR_LIGHT_GREY;
            set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
            vga_print_string(vga, panel_contents[p][i]);
        }
    }

    // 底部状态栏
    vga_draw_fill_rect(vga, 0, vga->height - 2, vga->width, 1, ' ', VGA_COLOR_WHITE,
                       VGA_COLOR_DARK_GREY);
    vga_set_cursor(vga, 2, vga->height - 2);
    fc = VGA_COLOR_WHITE;
    set_vga_property(vga, &fc, CURSOR_FONT_COLOR);
    vga_print_string(vga, "CCOS VGA Driver Demo - Ready!");

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
    vga_delay(400000000);

    // 2. 显示Logo
    vga_clear(vga, VGA_COLOR_BLACK);
    vga_show_logo(vga);
    vga_delay(400000000);

    // 3. 颜色表格演示
    vga_clear(vga, VGA_COLOR_BLACK);
    vga_color_table_demo(vga);
    vga_delay(400000000);

    // 4. 星空粒子效果
    vga_starfield_demo(vga);
    vga_delay(400000000);

    // 5. 进度条动画
    vga_clear(vga, VGA_COLOR_BLACK);
    vga_progress_bar_demo(vga);
    vga_delay(400000000);

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
#else
void vga_example_show() {
    int i = 0;
    (void)i;
    return;
}
#endif