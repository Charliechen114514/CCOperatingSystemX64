/**
 * @file fs.h
 * @author Charliechen114514 (chengh1922@mails.jlu.edu.cn)
 * @brief Main Filesystem Subsystem Header
 * @version 0.1
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 *
 * This header provides the main filesystem subsystem initialization and
 * includes for block devices, VFS, and filesystem drivers.
 */

#pragma once

#include "defines/types.h"

/* Include block device layer */
#include "block/block.h"

/* Include VFS layer */
#include "vfs/vfs.h"

/* ============================================================================
 * Main Filesystem Subsystem API
 * ============================================================================ */

/**
 * @brief Initialize the filesystem subsystem
 *
 * Initializes the block device layer, VFS, and all filesystem drivers.
 *
 * @return 0 on success, negative error code on failure
 */
int fs_init(void);

/**
 * @brief Get filesystem subsystem information
 */
void fs_info(void);
