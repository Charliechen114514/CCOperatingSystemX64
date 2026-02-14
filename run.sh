#!/bin/bash
# Test script for CCOS Bootloader

BOOT_IMG="build/boot.img"

echo "Building bootloader..."
make clean && make

if [ ! -f "$BOOT_IMG" ]; then
    echo "Error: Build failed - $BOOT_IMG not found!"
    exit 1
fi

echo "Running in QEMU (window should open)..."
echo "Close QEMU window to exit"
echo ""

# Try different display backends
if qemu-system-x86_64 -drive format=raw,file=$BOOT_IMG -display gtk 2>/dev/null; then
    echo "Booted successfully with GTK"
elif qemu-system-x86_64 -drive format=raw,file=$BOOT_IMG -vga std 2>/dev/null; then
    echo "Booted successfully with VGA"
else
    echo "No display backend available, trying with VNC..."
    echo "VNC will run on localhost:5900"
    qemu-system-x86_64 -drive format=raw,file=$BOOT_IMG -vnc :0
fi
