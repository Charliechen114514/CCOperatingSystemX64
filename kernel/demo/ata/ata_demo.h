/**
 * @file ata_demo.h
 * @brief ATA/IDE Disk Driver Demo - Demonstrates disk I/O functionality
 */

#pragma once

#include "defines/types.h"

/**
 * @brief Run all ATA demos
 *
 * This function demonstrates the various ATA capabilities:
 * 1. Reading MBR and verifying signature
 * 2. Reading multiple sectors
 * 3. Write and read back verification (safe area)
 * 4. Displaying device information
 *
 * Call this function after ATA initialization to see the demos.
 */
void ata_run_demo(void);
