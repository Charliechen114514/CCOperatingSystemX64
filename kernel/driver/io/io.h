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
uint8_t inb(uint16_t port);

/**
 * @brief Write target bytes to the port
 *
 * @param port
 * @param data
 */
void outb(uint16_t port, uint8_t data);