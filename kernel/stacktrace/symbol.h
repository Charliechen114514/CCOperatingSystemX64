/**
 * @file symbol.h
 * @brief Symbol table for stack trace symbol resolution
 * @date 2026-02-17
 */

#pragma once

#include "defines/types.h"

/**
 * @brief Symbol entry structure
 *
 * Each entry represents a function or symbol with its address and name.
 */
typedef struct {
    uintptr_t address;  /**< Symbol address */
    const char* name;   /**< Symbol name (pointer to string table) */
} symbol_entry_t;

/**
 * @brief Symbol table structure
 */
typedef struct {
    const symbol_entry_t* entries;  /**< Array of symbol entries */
    int count;                      /**< Number of symbols */
    uintptr_t text_start;           /**< Start of .text section */
    uintptr_t text_end;             /**< End of .text section */
} symbol_table_t;

/**
 * @brief Get the kernel symbol table
 *
 * @return Pointer to the symbol table
 */
const symbol_table_t* get_symbol_table(void);

/**
 * @brief Find symbol name for an address
 *
 * @param addr The address to look up
 * @param out_name Pointer to store the symbol name
 * @param out_offset Pointer to store the offset from symbol start
 * @return true if a symbol was found
 */
bool find_symbol(uintptr_t addr, const char** out_name, uintptr_t* out_offset);

/**
 * @brief Format address with symbol information
 *
 * Writes a string like "function_name + 0x12" to the buffer.
 *
 * @param buffer Output buffer
 * @param size Buffer size
 * @param addr Address to format
 */
void format_symbol(char* buffer, size_t size, uintptr_t addr);
