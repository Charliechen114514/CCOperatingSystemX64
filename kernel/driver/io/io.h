/**
 * @file io.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief IO Base Operations supports
 * @version 0.1
 * @date 2026-02-16
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once
#include "defines/types.h"

/**
 * @brief Read One Byte from the target port
 *
 * @param port
 * @return uint8_t
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/**
 * @brief Write target bytes to the port
 *
 * @param port
 * @param data
 */
static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}