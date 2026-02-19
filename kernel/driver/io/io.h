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
__attribute__((always_inline)) inline uint8_t inb(uint16_t port) {
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
__attribute__((always_inline)) inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

/**
 * @brief Read a word (16-bit) from the target port
 *
 * @param port
 * @return uint16_t
 */
__attribute__((always_inline)) inline uint16_t inw(uint16_t port) {
    uint16_t result;
    __asm__ volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/**
 * @brief Write a word (16-bit) to the port
 *
 * @param port
 * @param data
 */
__attribute__((always_inline)) inline void outw(uint16_t port, uint16_t data) {
    __asm__ volatile("outw %0, %1" : : "a"(data), "Nd"(port));
}

/**
 * @brief Read a dword (32-bit) from the target port
 *
 * @param port
 * @return uint32_t
 */
__attribute__((always_inline)) inline uint32_t inl(uint16_t port) {
    uint32_t result;
    __asm__ volatile("inl %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/**
 * @brief Write a dword (32-bit) to the port
 *
 * @param port
 * @param data
 */
__attribute__((always_inline)) inline void outl(uint16_t port, uint32_t data) {
    __asm__ volatile("outl %0, %1" : : "a"(data), "Nd"(port));
}

/**
 * @brief Read string of words from port
 *
 * @param port I/O port
 * @param addr Destination address
 * @param count Number of words to read
 */
static inline void insw(uint16_t port, void* addr, uint16_t count) {
    __asm__ volatile("rep insw"
                     : "+D"(addr), "+c"(count)
                     : "d"(port)
                     : "memory");
}

/**
 * @brief Write string of words to port
 *
 * @param port I/O port
 * @param addr Source address
 * @param count Number of words to write
 */
static inline void outsw(uint16_t port, const void* addr, uint16_t count) {
    __asm__ volatile("rep outsw"
                     : "+S"(addr), "+c"(count)
                     : "d"(port)
                     : "memory");
}
