#!/usr/bin/env python3
"""
CCOS Disk Layout Verification Script

This script verifies that the bootloader and kernel do not overlap in memory
and checks the disk layout for potential issues.

Usage:
    python3 scripts/verify_disk_layout.py build/bootloader.bin build/kernel.bin
"""

import sys
import struct
from pathlib import Path


def read_file(path):
    """Read binary file."""
    with open(path, 'rb') as f:
        return f.read()


def check_mbr_signature(data):
    """Check if data has valid MBR signature (0x55AA at offset 510)."""
    if len(data) < 512:
        return False, "File too small for MBR check"

    signature = struct.unpack('<H', data[510:512])[0]
    if signature != 0xAA55:
        return False, f"Invalid MBR signature: 0x{signature:04X} (expected 0xAA55)"

    return True, "Valid MBR signature"


def analyze_bootloader(data):
    """Analyze bootloader structure and size."""
    info = {
        'size': len(data),
        'sectors': (len(data) + 511) // 512,
        'has_mbr': False,
        'mbr_valid': False,
        'stage2_offset': None,
    }

    # Check MBR signature
    if len(data) >= 512:
        signature = struct.unpack('<H', data[510:512])[0]
        info['has_mbr'] = True
        info['mbr_valid'] = (signature == 0xAA55)

    # Look for Stage 2 markers
    # Stage 2 should start at offset 512 (sector 2)
    if len(data) > 512:
        # Check for GDT signature or common Stage 2 patterns
        # GDT null descriptor is 8 zero bytes
        if data[512:520] == b'\x00' * 8:
            info['stage2_offset'] = 512

    return info


def calculate_memory_layout(bootloader_sectors, kernel_sectors):
    """Calculate memory and disk layout to detect overlaps."""
    layout = {
        'disk': {},
        'memory': {},
        'warnings': [],
        'errors': [],
    }

    # Disk layout (LBA)
    layout['disk']['bootloader_start'] = 0  # LBA 0 (sector 1)
    layout['disk']['bootloader_end'] = bootloader_sectors - 1
    layout['disk']['kernel_start'] = bootloader_sectors
    layout['disk']['kernel_end'] = bootloader_sectors + kernel_sectors - 1

    # Memory layout
    # Stage 1: 0x7C00
    # Stage 2: 0x7E00
    # Kernel: 0x10000

    stage2_end = 0x7E00 + (bootloader_sectors * 512)
    kernel_start = 0x10000

    layout['memory']['stage1'] = (0x7C00, 0x7E00)
    layout['memory']['stage2'] = (0x7E00, stage2_end)
    layout['memory']['kernel'] = (kernel_start, kernel_start + kernel_sectors * 512)

    # Check for memory overlap
    if stage2_end > kernel_start:
        layout['errors'].append(
            f"Memory overlap: Stage 2 ends at 0x{stage2_end:X}, "
            f"kernel starts at 0x{kernel_start:X}"
        )
    elif (kernel_start - stage2_end) < 512:
        layout['warnings'].append(
            f"Small memory gap: {kernel_start - stage2_end} bytes between "
            f"Stage 2 (0x{stage2_end:X}) and kernel (0x{kernel_start:X})"
        )

    # Check kernel size (max 64MB)
    max_kernel_sectors = (64 * 1024 * 1024) // 512
    if kernel_sectors > max_kernel_sectors:
        layout['errors'].append(
            f"Kernel too large: {kernel_sectors} sectors ({kernel_sectors * 512 // (1024*1024)}MB), "
            f"max {max_kernel_sectors} sectors (64MB)"
        )

    return layout


def print_layout(layout):
    """Print formatted layout information."""
    print("\n" + "=" * 60)
    print("DISK LAYOUT (LBA)")
    print("=" * 60)
    print(f"Bootloader:  Sector 1-{layout['disk']['bootloader_end'] + 1} "
          f"(LBA 0-{layout['disk']['bootloader_end']})")
    print(f"Kernel:      Sector {layout['disk']['kernel_start'] + 1}-"
          f"{layout['disk']['kernel_end'] + 1} "
          f"(LBA {layout['disk']['kernel_start']}-{layout['disk']['kernel_end']})")

    print("\n" + "=" * 60)
    print("MEMORY LAYOUT")
    print("=" * 60)
    stage1 = layout['memory']['stage1']
    stage2 = layout['memory']['stage2']
    kernel = layout['memory']['kernel']
    print(f"Stage 1:     0x{stage1[0]:04X} - 0x{stage1[1]:04X}")
    print(f"Stage 2:     0x{stage2[0]:04X} - 0x{stage2[1]:04X}")
    print(f"Kernel:      0x{kernel[0]:04X} - 0x{kernel[1]:04X}")

    # Calculate gaps
    stage2_size = stage2[1] - stage2[0]
    gap = kernel[0] - stage2[1]
    print(f"\nStage 2 size: {stage2_size} bytes ({stage2_size // 512} sectors)")
    print(f"Memory gap:  {gap} bytes ({gap // 512} sectors)")

    if layout['warnings']:
        print("\n" + "=" * 60)
        print("WARNINGS")
        print("=" * 60)
        for warning in layout['warnings']:
            print(f"  ⚠ {warning}")

    if layout['errors']:
        print("\n" + "=" * 60)
        print("ERRORS")
        print("=" * 60)
        for error in layout['errors']:
            print(f"  ✗ {error}")
        return False

    return True


def main():
    if len(sys.argv) < 3:
        print("Usage: python3 verify_disk_layout.py <bootloader.bin> <kernel.bin>")
        sys.exit(1)

    bootloader_path = Path(sys.argv[1])
    kernel_path = Path(sys.argv[2])

    if not bootloader_path.exists():
        print(f"Error: Bootloader not found: {bootloader_path}")
        sys.exit(1)

    if not kernel_path.exists():
        print(f"Error: Kernel not found: {kernel_path}")
        sys.exit(1)

    print("CCOS Disk Layout Verification")
    print("=" * 60)

    # Read files
    bootloader_data = read_file(bootloader_path)
    kernel_data = read_file(kernel_path)

    # Analyze bootloader
    print("\nAnalyzing bootloader...")
    boot_info = analyze_bootloader(bootloader_data)
    print(f"  Size: {boot_info['size']} bytes ({boot_info['sectors']} sectors)")
    print(f"  Has MBR: {boot_info['has_mbr']}")
    if boot_info['has_mbr']:
        valid, msg = check_mbr_signature(bootloader_data)
        boot_info['mbr_valid'] = valid
        print(f"  {msg}")

    # Analyze kernel
    print("\nAnalyzing kernel...")
    kernel_sectors = (len(kernel_data) + 511) // 512
    kernel_size_mb = len(kernel_data) / (1024 * 1024)
    print(f"  Size: {len(kernel_data)} bytes ({kernel_sectors} sectors, {kernel_size_mb:.2f} MB)")

    # Calculate layout
    layout = calculate_memory_layout(boot_info['sectors'], kernel_sectors)

    # Print results
    success = print_layout(layout)

    # Check for magic numbers in kernel
    print("\n" + "=" * 60)
    print("KERNEL MAGIC NUMBERS")
    print("=" * 60)

    # Check for common magic numbers at kernel start
    if len(kernel_data) >= 4:
        first_dwords = struct.unpack('<I', kernel_data[:4])[0]
        print(f"  First dword: 0x{first_dwords:08X}")

        # Check for ELF magic (if it's an ELF file by mistake)
        if kernel_data[:4] == b'\x7FELF':
            print("  ⚠ Warning: Kernel appears to be an ELF file, should be raw binary")

    # Check for expected kernel entry pattern
    # 64-bit kernel typically starts with specific opcodes
    if len(kernel_data) >= 3:
        if kernel_data[0] == 0x48:  # REX.W prefix common in x86-64
            print("  ✓ Appears to be x86-64 code (REX prefix found)")

    print("\n" + "=" * 60)
    if success:
        print("✓ PASSED: Layout is valid")
        return 0
    else:
        print("✗ FAILED: Layout has errors")
        return 1


if __name__ == '__main__':
    sys.exit(main())
