/**
 * @file vfs_demo.h
 * @brief VFS/EXT2 Filesystem Demo - Demonstrates file and directory operations
 */

#pragma once

#include "defines/types.h"

/**
 * @brief Run all VFS/EXT2 filesystem demos
 *
 * This function demonstrates the VFS layer with EXT2 filesystem:
 * 1. Mounting EXT2 filesystem
 * 2. Listing root directory contents
 * 3. Reading a file
 * 4. Directory traversal
 * 5. File statistics
 *
 * Call this function after filesystem initialization to see the demos.
 */
void vfs_run_demo(void);
