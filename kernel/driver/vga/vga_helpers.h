#pragma once
#include "defines/types.h"
#include "vga.h"

static inline vga_sz_t vga_cursor_x(const CCOS_VGA* vga){
    // Get the High uint8 as X Position
    return (vga_sz_t)((vga->native_cursor_pos & 0xFF00) >> 8);
}

static inline vga_sz_t vga_cursor_y(const CCOS_VGA* vga){
    // Get the Low uint8 as Y position
    return (vga_sz_t)(vga->native_cursor_pos & 0x00FF);
}