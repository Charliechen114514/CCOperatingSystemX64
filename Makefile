# Makefile for CCOS Bootloader + Kernel

AS = nasm
ASFLAGS = -f bin
QEMU = qemu-system-x86_64

# Directories
SRC_DIR = boot
KERNEL_DIR = kernel
BUILD_DIR = build

# Source files
BOOTLOADER_ASM = $(SRC_DIR)/bootloader.asm
KERNEL_ASM = $(KERNEL_DIR)/kernel.asm

# Output files in build directory
BOOTLOADER_BIN = $(BUILD_DIR)/bootloader.bin
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
BOOT_IMG = $(BUILD_DIR)/boot.img

.PHONY: all clean run debug prepare

all: $(BOOT_IMG)

# Prepare build directory
prepare:
	@mkdir -p $(BUILD_DIR)

# Build unified bootloader (contains both stage 1 and stage 2)
# The output will have stage 1 at offset 0 and stage 2 at offset 512 (0x200)
$(BOOTLOADER_BIN): $(BOOTLOADER_ASM) | prepare
	@echo "Building unified bootloader..."
	$(AS) $(ASFLAGS) $< -o $@
	@echo "Bootloader size:"
	@ls -lh $@

# Build kernel
$(KERNEL_BIN): $(KERNEL_ASM) | prepare
	@echo "Building Kernel..."
	$(AS) $(ASFLAGS) $< -o $@
	@echo "Kernel size:"
	@ls -lh $@

# Combine bootloader and kernel into boot image
# bootloader: sectors 1-2 (stage1 + stage2), kernel: sector 3+
$(BOOT_IMG): $(BOOTLOADER_BIN) $(KERNEL_BIN)
	@echo "Combining bootloader and kernel into boot image..."
	dd if=$(BOOTLOADER_BIN) of=$@ bs=512 count=2 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=2 conv=notrunc 2>/dev/null
	@echo "Final boot image:"
	@ls -lh $@
	@echo "Bootloader (Stage1+2): sectors 1-2"
	@echo "Kernel: sector 3+"
	@echo "Done! Boot image: $@"

clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)
	@rm -f serial.log
	@echo "Clean done."

run: $(BOOT_IMG)
	@echo "Running with QEMU (hard drive mode)..."
	$(QEMU) -drive format=raw,file=$(BOOT_IMG),if=ide -nographic

debug: $(BOOT_IMG)
	@echo "Running with QEMU (debug, hard drive mode)..."
	$(QEMU) -drive format=raw,file=$(BOOT_IMG),if=ide -nographic -s -S
