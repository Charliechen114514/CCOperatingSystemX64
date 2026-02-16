#pragma once

/**
 * @brief   log the informations to the vga backends
 *
 * @param file     Source file name where assert failed
 * @param line     Line number where assert failed
 * @param func     Function name where assert failed
 * @param expr_str Stringified expression that failed
 */
void assert_backend_to_vga(const char* file, int line, const char* func,
                           const char* expr_str);

/**
 * @brief   After log to somewhere, kernel is expected to be hanged
 *          on
 *
 */
void assert_failed_action(void);