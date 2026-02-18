#!/usr/bin/env python3
"""
==============================================================================
CCOS - E820 Memory Configuration Validator for x86_64
==============================================================================
This script validates the safety of E820_STORAGE_ADDR configuration for
an x86_64 OS that boots via:

  1. Real Mode (0x7C00) - Stage 1 MBR bootloader
  2. Real Mode (0x7E00) - Stage 2 bootloader
  3. Protected Mode - Transition to long mode
  4. Long Mode - Kernel execution at 0x10000

Memory Layout (Low 1MB Conventional Memory):
  ┌─────────────────────────────────────────────────────────────────┐
  │ 0x000000 - 0x0004FF │ IVT & BDA         │ Interrupt Vector Table │
  ├─────────────────────────────────────────────────────────────────┤
  │ 0x000500 - 0x07BFF │ Available         │ Free for use           │
  ├─────────────────────────────────────────────────────────────────┤
  │ 0x07C00 - 0x07DFF │ Stage 1 MBR       │ Bootloader entry       │
  ├─────────────────────────────────────────────────────────────────┤
  │ 0x07E00 - 0x08BFF │ Stage 2           │ Main bootloader code   │
  ├─────────────────────────────────────────────────────────────────┤
  │ 0x08C00 - 0x08FFF │ Available         │ Free gap (1KB)         │
  ├─────────────────────────────────────────────────────────────────┤
  │ 0x09000 - 0x09FFF │ PML4              │ Page Map Level 4 (4KB) │
  ├─────────────────────────────────────────────────────────────────┤
  │ 0x0A000 - 0x0AFFF │ PDPT              │ Page Dir Ptr Table (4KB)│
  ├─────────────────────────────────────────────────────────────────┤
  │ 0x0B000 - 0x0BFFF │ PD                │ Page Directory (4KB)   │
  ├─────────────────────────────────────────────────────────────────┤
  │ 0x0C000 - 0x0FFFF │ Available         │ Free (16KB gap)        │
  ├─────────────────────────────────────────────────────────────────┤
  │ 0x10000 - 0xXXXXX │ Kernel            │ OS kernel loaded here  │
  └─────────────────────────────────────────────────────────────────┘

Important Notes for x86_64:
  - E820 data is stored in real mode BEFORE paging is enabled
  - Must be accessible in real mode (addressable with 16-bit segment:offset)
  - Must NOT overlap with page tables (they map first 2MB/4MB identity)
  - Kernel expects to find data at the configured address

Usage:
    python validate_e820_config.py [ADDRESS] [ENTRIES]
    python validate_e820_config.py              # Use CMake defaults
    python validate_e820_config.py 0x6000 128   # Custom values
==============================================================================
"""

import sys
import re
from pathlib import Path
from typing import List, Tuple, Optional, Dict
from dataclasses import dataclass


@dataclass
class MemoryRegion:
    """Represents a reserved memory region."""
    name: str
    start: int
    end: int
    purpose: str
    critical: bool = True  # If True, overlap causes fatal error

    @property
    def size(self) -> int:
        return self.end - self.start

    @property
    def size_kb(self) -> float:
        return self.size / 1024

    def overlaps(self, other_start: int, other_end: int) -> bool:
        return not (self.end <= other_start or self.start >= other_end)

    def contains(self, address: int) -> bool:
        return self.start <= address < self.end

    def __str__(self) -> str:
        critical_mark = "!" if self.critical else " "
        return (f"{critical_mark} {self.name:22s} 0x{self.start:05X} - 0x{self.end:05X} "
                f"({self.size_kb:6.2f} KB)  {self.purpose}")


class x86_64MemoryLayout:
    """x86_64 bootloader memory layout."""

    E820_ENTRY_SIZE = 24  # bytes per E820 entry

    # Real mode addressability limit (with segment:offset)
    # Max address = 0xFFFF:0xFFFF = 0x10FFEF (with A20 gate)
    REAL_MODE_LIMIT = 0x100000  # 1MB

    def __init__(self, bootloader_sectors: int = 6, kernel_size: int = 64 * 1024):
        """
        Initialize memory layout.

        Args:
            bootloader_sectors: Number of sectors bootloader uses
            kernel_size: Estimated kernel size in bytes
        """
        self.regions: List[MemoryRegion] = []
        self.bootloader_sectors = bootloader_sectors
        self.kernel_size = kernel_size
        self._build_layout()

    def _build_layout(self):
        """Build the complete memory layout."""
        # Calculate Stage 2 end address
        stage2_start = 0x7E00
        stage2_size = self.bootloader_sectors * 512
        stage2_end = stage2_start + stage2_size

        # ========================================================================
        # Reserved Regions (in order of address)
        # ========================================================================

        # 1. BIOS Data Area (BDA) - Contains IVT at 0x0000
        self.regions.append(MemoryRegion(
            "BIOS Data Area", 0x0000, 0x0500,
            "Interrupt Vector Table (0x0000-0x03FF) and BDA (0x0400-0x04FF)",
            critical=True
        ))

        # 2. Free Region: 0x0500 - 0x7C00 (30KB available)
        #    This is a good candidate location for E820 storage!

        # 3. Stage 1 MBR Bootloader
        self.regions.append(MemoryRegion(
            "Stage 1 MBR", 0x7C00, 0x7E00,
            "MBR bootloader loaded by BIOS",
            critical=True
        ))

        # 4. Stage 2 Bootloader (variable size)
        self.regions.append(MemoryRegion(
            "Stage 2 Bootloader", stage2_start, stage2_end,
            f"Main bootloader ({self.bootloader_sectors} sectors = {stage2_size} bytes)",
            critical=True
        ))

        # 5. Gap after Stage 2 before page tables
        #    stage2_end to 0x9000

        # 6. Page Tables - MUST be identity mapped in first 4MB
        #    PML4 at 0x9000, PDPT at 0xA000, PD at 0xB000
        self.regions.append(MemoryRegion(
            "Page Tables", 0x9000, 0xC000,
            "PML4 (0x9000), PDPT (0xA000), PD (0xB000) - Identity mapped",
            critical=True
        ))

        # 7. Gap after page tables: 0xC000 - 0x10000 (16KB available)
        #    Also a good candidate for E820 storage!

        # 8. Kernel Load Area
        kernel_end = 0x10000 + self.kernel_size
        self.regions.append(MemoryRegion(
            "Kernel", 0x10000, kernel_end,
            f"Kernel loaded at 0x10000 (~{self.kernel_size//1024}KB)",
            critical=True
        ))

        # 9. Conventional Memory Top / Extended Memory BIOS Data Area
        #    Starts at 0xA0000 (640KB) for VGA, etc.

        # 10. VGA Text Mode Buffer (if in use)
        self.regions.append(MemoryRegion(
            "VGA Buffer", 0xB8000, 0xC0000,
            "VGA text mode video memory",
            critical=False
        ))

        # 11. BIOS ROM Area
        self.regions.append(MemoryRegion(
            "BIOS ROM", 0xF0000, 0x100000,
            "System BIOS (also copied to first 1MB)",
            critical=False
        ))

    def get_region_at(self, address: int) -> Optional[MemoryRegion]:
        """Get the region containing the given address."""
        for region in self.regions:
            if region.contains(address):
                return region
        return None

    def get_overlapping_regions(self, start: int, end: int) -> List[MemoryRegion]:
        """Get all regions that overlap with the given range."""
        return [r for r in self.regions if r.overlaps(start, end)]

    def find_safe_regions(self, required_size: int) -> List[Dict]:
        """
        Find safe regions for E820 storage.

        Returns list of dicts with: start, end, size_kb, description
        """
        safe_regions = []

        # Sort regions by start address
        sorted_regions = sorted(self.regions, key=lambda r: r.start)

        # Find gaps between regions
        for i in range(len(sorted_regions)):
            if i == 0:
                gap_start = 0x0000
            else:
                gap_start = sorted_regions[i-1].end

            gap_end = sorted_regions[i].start

            # Only consider gaps below 1MB for real mode safety
            if gap_end > self.REAL_MODE_LIMIT:
                continue

            gap_size = gap_end - gap_start

            if gap_size >= required_size:
                # Find aligned address within gap
                aligned_start = ((gap_start + 15) // 16) * 16

                if aligned_start + required_size <= gap_end:
                    safe_regions.append({
                        'start': aligned_start,
                        'end': aligned_start + required_size,
                        'size_kb': required_size / 1024,
                        'gap_start': gap_start,
                        'gap_end': gap_end,
                        'gap_size_kb': gap_size / 1024,
                        'description': self._describe_gap_location(gap_start, gap_end)
                    })

        return safe_regions

    def _describe_gap_location(self, start: int, end: int) -> str:
        """Describe where a gap is located."""
        if start < 0x500:
            return "In BIOS Data Area (unsafe)"
        elif start < 0x7C00:
            return "Between BDA and Stage 1 MBR"
        elif end <= 0x9000:
            return "Between Stage 2 and Page Tables"
        elif end <= 0x10000:
            return "Between Page Tables and Kernel"
        else:
            return "Above kernel load area"

    def validate_e820_config(self, storage_addr: int, max_entries: int) -> Dict:
        """
        Validate E820 configuration.

        Returns dict with:
            - is_safe: bool
            - errors: list of error messages
            - warnings: list of warning messages
            - info: list of info messages
        """
        result = {
            'is_safe': True,
            'errors': [],
            'warnings': [],
            'info': []
        }

        storage_size = max_entries * self.E820_ENTRY_SIZE + 3  # +3 for header
        storage_end = storage_addr + storage_size

        # Basic checks
        if storage_addr < 0:
            result['errors'].append("ERROR: Negative address")
            result['is_safe'] = False
            return result

        if storage_addr >= self.REAL_MODE_LIMIT:
            result['errors'].append(
                f"ERROR: Address 0x{storage_addr:X} is above 1MB - "
                "not accessible in real mode without special handling"
            )
            result['is_safe'] = False

        if max_entries < 1 or max_entries > 256:
            result['errors'].append(
                f"ERROR: max_entries={max_entries} is out of range (1-256)"
            )
            result['is_safe'] = False

        # Alignment check
        if storage_addr & 0xF:  # Not 16-byte aligned
            result['warnings'].append(
                f"WARNING: Address 0x{storage_addr:X} is not 16-byte aligned"
            )

        # Check for overlaps with critical regions
        overlapping = self.get_overlapping_regions(storage_addr, storage_end)

        for region in overlapping:
            if region.critical:
                result['errors'].append(
                    f"ERROR: Overlaps with critical region '{region.name}' "
                    f"(0x{region.start:X}-0x{region.end:X})"
                )
                result['is_safe'] = False
            else:
                result['warnings'].append(
                    f"WARNING: Overlaps with '{region.name}' "
                    f"(0x{region.start:X}-0x{region.end:X})"
                )

        # Check if near boundaries
        for region in self.regions:
            if storage_addr < region.end < storage_end:
                result['warnings'].append(
                    f"WARNING: Storage straddles boundary of '{region.name}'"
                )

        # Page table identity mapping consideration
        # Page tables at 0x9000-0xBFFF map the first 2MB-4MB
        # Data in this range will be identity mapped
        if storage_addr >= 0x9000 and storage_addr < 0xC000:
            result['info'].append(
                "INFO: Storage is in page table area - will be identity mapped"
            )

        return result

    def print_layout(self, highlight_addr: int = None, highlight_size: int = 0):
        """Print the memory layout."""
        print("\n" + "=" * 80)
        print("x86_64 BOOTLOADER MEMORY LAYOUT (Low 1MB)")
        print("=" * 80)
        print(f"{'Marker':>2s} {'Region':<22s} {'Start':>8s} {'End':>8s} {'Size':>10s} {'Purpose'}")
        print("-" * 80)

        for region in sorted(self.regions, key=lambda r: r.start):
            # Check if this region should be highlighted
            marker = "  "
            if highlight_addr is not None:
                region_end = highlight_addr + highlight_size
                if region.overlaps(highlight_addr, region_end):
                    marker = ">>"

            print(f"{marker} {region.name:22s} 0x{region.start:05X}   "
                  f"0x{region.end:05X}   {region.size_kb:6.2f} KB  {region.purpose}")

        # Show E820 storage if specified
        if highlight_addr is not None and highlight_size > 0:
            print("-" * 80)
            print(f">> {'E820 Storage':22s} 0x{highlight_addr:05X}   "
                  f"0x{highlight_addr + highlight_size:05X}   "
                  f"{highlight_size/1024:6.2f} KB  Your configuration")

        print("=" * 80)


def parse_hex(value: str) -> int:
    """Parse hexadecimal value with optional 0x prefix."""
    value = value.strip().upper()
    if value.startswith("0X"):
        return int(value, 16)
    if value.endswith("H"):
        return int(value[:-1], 16)
    return int(value, 16) if value else 0


def read_cmake_config() -> Tuple[int, int]:
    """Read default values from cmake/MemConfig.cmake."""
    cmake_file = Path(__file__).parent.parent / "cmake" / "MemConfig.cmake"

    default_addr = 0x6000
    default_entries = 128

    if not cmake_file.exists():
        return default_addr, default_entries

    with open(cmake_file, 'r') as f:
        content = f.read()

        match = re.search(r'E820_STORAGE_ADDR_DEFAULT\s+"(0x[0-9A-Fa-f]+)"', content)
        if match:
            default_addr = int(match.group(1), 16)

        match = re.search(r'E820_MAX_ENTRIES_DEFAULT\s+"(\d+)"', content)
        if match:
            default_entries = int(match.group(1))

    return default_addr, default_entries


def main():
    """Main entry point."""
    import argparse

    parser = argparse.ArgumentParser(
        description="Validate E820 memory configuration for x86_64",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument("address", nargs="?", help="E820 storage address (hex)")
    parser.add_argument("entries", nargs="?", type=int, help="Max E820 entries")
    parser.add_argument("-c", "--cmake", action="store_true",
                        help="Use defaults from cmake/MemConfig.cmake")
    parser.add_argument("-q", "--quiet", action="store_true",
                        help="Quiet mode, minimal output")
    parser.add_argument("-b", "--bootloader-sectors", type=int, default=6,
                        help="Bootloader size in sectors (default: 6)")
    parser.add_argument("-k", "--kernel-size", type=int, default=64*1024,
                        help="Kernel size in bytes (default: 64KB)")

    args = parser.parse_args()

    # Get configuration values
    if args.cmake or args.address is None:
        storage_addr, max_entries = read_cmake_config()
    else:
        storage_addr = parse_hex(args.address) if args.address else 0x6000
        max_entries = args.entries if args.entries else 128

    # Create memory layout
    layout = x86_64MemoryLayout(args.bootloader_sectors, args.kernel_size)

    # Validate configuration
    validation = layout.validate_e820_config(storage_addr, max_entries)
    storage_size = max_entries * x86_64MemoryLayout.E820_ENTRY_SIZE + 3

    if not args.quiet:
        print("\n" + "=" * 80)
        print("E820 CONFIGURATION VALIDATION")
        print("=" * 80)
        print(f"E820 Storage Address:  0x{storage_addr:05X}")
        print(f"E820 Max Entries:      {max_entries}")
        print(f"Storage Size:          {storage_size} bytes ({storage_size/1024:.2f} KB)")
        print(f"Storage Range:         0x{storage_addr:05X} - 0x{storage_addr + storage_size:05X}")
        print("=" * 80)

    # Print errors (always, even in quiet mode)
    for msg in validation['errors']:
        print(msg, file=sys.stderr)

    if not args.quiet:
        # Print warnings and info only in verbose mode
        for msg in validation['warnings']:
            print(msg)
        for msg in validation['info']:
            print(msg)

        # Show layout with E820 highlighted
        layout.print_layout(storage_addr, storage_size)

        # Show safe alternatives
        if not validation['is_safe']:
            print("\nSAFE ALTERNATIVE ADDRESSES:")
            print("-" * 80)
            safe_regions = layout.find_safe_regions(storage_size)

            if safe_regions:
                for i, region in enumerate(safe_regions[:3], 1):
                    print(f"  {i}. 0x{region['start']:05X} - {region['description']}")
                    print(f"      Gap: 0x{region['gap_start']:05X}-0x{region['gap_end']:05X} "
                          f"({region['gap_size_kb']:.2f} KB available)")
            else:
                print("  No safe regions found - reduce max_entries or bootloader size")

            print("=" * 80)

        # Final verdict
        if validation['is_safe']:
            print("\n✓ Configuration is SAFE\n")
        else:
            print("\n✗ Configuration is UNSAFE\n")

    sys.exit(0 if validation['is_safe'] else 1)


if __name__ == "__main__":
    main()
