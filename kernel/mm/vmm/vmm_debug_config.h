/* ==============================================================================
 * CCOS - VMM Debug Configuration
 * ==============================================================================
 * This header controls the scanning behavior of vmm_dump_memory_map().
 * Adjust these values to control how much of the page table hierarchy is scanned.
 * ==============================================================================
 */

#pragma once

/* ============================================================================
 * Memory Dump Scan Configuration
 * ============================================================================
 * These values control how many entries are scanned at each level of the
 * page table hierarchy. Lower values = faster scanning, less output.
 * ============================================================================ */

/* Maximum number of mapping entries to collect */
#define VMM_DEBUG_MAX_MAPPINGS         256

/* PDPT (Page Directory Pointer Table) scan range
 * - Valid range: 1-512
 * - 512 = scan all entries (recommended for full dump)
 */
#define VMM_DEBUG_MAX_PDPT_ENTRIES     512

/* PD (Page Directory) scan range per PDPT entry
 * - Valid range: 1-512
 * - 512 = scan all entries (can be very slow)
 * - 64  = reasonable default for low memory regions
 */
#define VMM_DEBUG_MAX_PD_ENTRIES       512

/* PT (Page Table) scan range per PD entry
 * - Valid range: 1-512
 * - 512 = scan all entries (very slow for large memory maps)
 * - 16  = quick scan for demo/testing
 */
#define VMM_DEBUG_MAX_PT_ENTRIES       512

/* Default number of entries to display if max_entries is 0 */
#define VMM_DEBUG_DEFAULT_DISPLAY      64
