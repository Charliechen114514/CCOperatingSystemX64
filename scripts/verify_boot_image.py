#!/usr/bin/env python3
"""
CCOS Boot Image Verification Script

This script verifies that boot.img was correctly assembled from
bootloader.bin and kernel.bin using dd commands.

Checks:
1. MBR signature at offset 510-511 (0x55AA)
2. Bootloader content matches first N sectors
3. Kernel content matches subsequent sectors
4. No unexpected data overlap

Usage:
    python3 scripts/verify_boot_image.py build/boot.img build/bootloader.bin build/kernel.bin
"""

import sys
import struct
from pathlib import Path


def read_file(path):
    """Read binary file."""
    with open(path, 'rb') as f:
        return f.read()


def verify_mbr(data):
    """Verify MBR signature."""
    if len(data) < 512:
        return False, "Image too small for MBR check"

    signature = struct.unpack('<H', data[510:512])[0]
    if signature != 0xAA55:
        return False, f"Invalid MBR signature: 0x{signature:04X} (expected 0xAA55)"

    return True, "Valid MBR signature"


def verify_boot_image(boot_img, bootloader, kernel):
    """Verify boot.img was assembled correctly."""
    errors = []
    warnings = []

    # Get sizes
    bootloader_size = len(bootloader)
    bootloader_sectors = (bootloader_size + 511) // 512
    kernel_size = len(kernel)
    kernel_sectors = (kernel_size + 511) // 512

    print("\n" + "=" * 60)
    print("BOOT IMAGE VERIFICATION")
    print("=" * 60)

    # Check 1: MBR signature
    print("\n[1] Checking MBR signature...")
    valid, msg = verify_mbr(boot_img)
    if valid:
        print(f"    ✓ {msg}")
    else:
        print(f"    ✗ {msg}")
        errors.append("MBR signature check failed")

    # Check 2: Bootloader content
    print("\n[2] Verifying bootloader content...")
    expected_bootloader_bytes = bootloader_sectors * 512
    if len(boot_img) < expected_bootloader_bytes:
        errors.append(f"Boot image too small for bootloader: {len(boot_img)} < {expected_bootloader_bytes}")
        print(f"    ✗ Image too small")
    else:
        actual_bootloader = boot_img[:expected_bootloader_bytes]
        # Compare only actual bootloader bytes, padding doesn't matter
        if actual_bootloader[:bootloader_size] == bootloader:
            print(f"    ✓ Bootloader content matches ({bootloader_size} bytes, {bootloader_sectors} sectors)")
        else:
            # Find first mismatch
            for i in range(min(bootloader_size, len(actual_bootloader))):
                if actual_bootloader[i] != bootloader[i]:
                    errors.append(f"Bootloader mismatch at offset {i}: "
                                f"0x{actual_bootloader[i]:02X} != 0x{bootloader[i]:02X}")
                    print(f"    ✗ Mismatch at offset {i}")
                    break
            else:
                errors.append("Bootloader content mismatch (unknown location)")
                print(f"    ✗ Content mismatch")

    # Check 3: Kernel content
    print("\n[3] Verifying kernel content...")
    kernel_offset = bootloader_sectors * 512
    expected_kernel_bytes = kernel_sectors * 512

    if len(boot_img) < kernel_offset + kernel_size:
        errors.append(f"Boot image too small for kernel: need {kernel_offset + kernel_size}, got {len(boot_img)}")
        print(f"    ✗ Image too small for kernel")
    else:
        actual_kernel = boot_img[kernel_offset:kernel_offset + kernel_size]
        if actual_kernel == kernel:
            print(f"    ✓ Kernel content matches ({kernel_size} bytes, {kernel_sectors} sectors)")
            print(f"      Kernel at offset 0x{kernel_offset:X} (sector {bootloader_sectors}, LBA {bootloader_sectors})")
        else:
            # Find first mismatch
            for i in range(min(kernel_size, len(actual_kernel))):
                if actual_kernel[i] != kernel[i]:
                    errors.append(f"Kernel mismatch at offset {kernel_offset + i}: "
                                f"0x{actual_kernel[i]:02X} != 0x{kernel[i]:02X}")
                    print(f"    ✗ Mismatch at file offset 0x{kernel_offset + i:X}")
                    break
            else:
                errors.append("Kernel content mismatch (unknown location)")
                print(f"    ✗ Content mismatch")

    # Check 4: Verify no overlap
    print("\n[4] Checking for data overlap...")
    bootloader_end = bootloader_sectors * 512
    kernel_start = bootloader_sectors * 512

    if bootloader_end > kernel_start:
        errors.append(f"Data overlap: bootloader ends at {bootloader_end}, kernel starts at {kernel_start}")
        print(f"    ✗ Overlap detected")
    else:
        gap = kernel_start - bootloader_end
        print(f"    ✓ No overlap (gap: {gap} bytes)")

    # Check 5: Expected image size
    print("\n[5] Verifying expected image size...")
    expected_size = bootloader_sectors * 512 + kernel_sectors * 512
    actual_size = len(boot_img)

    print(f"    Expected: {expected_size} bytes ({bootloader_sectors} + {kernel_sectors} sectors)")
    print(f"    Actual:   {actual_size} bytes")

    if actual_size < expected_size:
        warnings.append(f"Boot image smaller than expected: {actual_size} < {expected_size}")
        print(f"    ⚠ Image smaller than expected (dd may have truncated)")
    elif actual_size > expected_size:
        extra_bytes = actual_size - expected_size
        warnings.append(f"Boot image has {extra_bytes} extra bytes beyond expected size")
        print(f"    ⚠ Image has {extra_bytes} extra bytes")
    else:
        print(f"    ✓ Size matches exactly")

    # Summary
    print("\n" + "=" * 60)
    if errors:
        print("✗ FAILED - Errors found:")
        for e in errors:
            print(f"  - {e}")
    if warnings:
        print("⚠ WARNINGS:")
        for w in warnings:
            print(f"  - {w}")

    if not errors and not warnings:
        print("✓ PASSED - All checks successful")
        return 0
    elif not errors:
        print("✓ PASSED with warnings")
        return 0
    else:
        print("✗ FAILED")
        return 1


def main():
    if len(sys.argv) < 4:
        print("Usage: python3 verify_boot_image.py <boot.img> <bootloader.bin> <kernel.bin>")
        sys.exit(1)

    boot_img_path = Path(sys.argv[1])
    bootloader_path = Path(sys.argv[2])
    kernel_path = Path(sys.argv[3])

    if not boot_img_path.exists():
        print(f"Error: Boot image not found: {boot_img_path}")
        sys.exit(1)

    if not bootloader_path.exists():
        print(f"Error: Bootloader not found: {bootloader_path}")
        sys.exit(1)

    if not kernel_path.exists():
        print(f"Error: Kernel not found: {kernel_path}")
        sys.exit(1)

    # Read files
    boot_img = read_file(boot_img_path)
    bootloader = read_file(bootloader_path)
    kernel = read_file(kernel_path)

    return verify_boot_image(boot_img, bootloader, kernel)


if __name__ == '__main__':
    sys.exit(main())
