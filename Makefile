# Makefile for CCOS Bootloader (Two-Stage) + Kernel

AS = nasm
ASFLAGS = -f bin
QEMU = qemu-system-x86_64

# Directories
SRC_DIR = boot
KERNEL_DIR = kernel
BUILD_DIR = build

# Source files
BOOT_ASM = $(SRC_DIR)/boot.asm
BOOT2_ASM = $(SRC_DIR)/boot2.asm
KERNEL_ASM = $(KERNEL_DIR)/kernel.asm

# Output files in build directory
BOOT_BIN = $(BUILD_DIR)/boot.bin
BOOT2_BIN = $(BUILD_DIR)/boot2.bin
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
BOOT_IMG = $(BUILD_DIR)/boot.img

.PHONY: all clean run debug prepare

all: $(BOOT_IMG)

# Prepare build directory
prepare:
	@mkdir -p $(BUILD_DIR)

# Build stage 1 (MBR)
$(BOOT_BIN): $(BOOT_ASM) | prepare
	@echo "Building Stage 1 (MBR)..."
	$(AS) $(ASFLAGS) $< -o $@
	@echo "Stage 1 size:"
	@ls -lh $@

# Build stage 2
$(BOOT2_BIN): $(BOOT2_ASM) | prepare
	@echo "Building Stage 2..."
	$(AS) $(ASFLAGS) $< -o $@
	@echo "Stage 2 size:"
	@ls -lh $@

# Build kernel
$(KERNEL_BIN): $(KERNEL_ASM) | prepare
	@echo "Building Kernel..."
	$(AS) $(ASFLAGS) $< -o $@
	@echo "Kernel size:"
	@ls -lh $@

# Combine all stages into boot image
# boot1: sector 1, boot2: sectors 2-3 (740 bytes), kernel: sector 4+
$(BOOT_IMG): $(BOOT_BIN) $(BOOT2_BIN) $(KERNEL_BIN)
	@echo "Combining stages into boot image..."
	dd if=$(BOOT_BIN) of=$@ bs=512 count=1 conv=notrunc 2>/dev/null
	dd if=$(BOOT2_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=3 conv=notrunc 2>/dev/null
	@echo "Final boot image:"
	@ls -lh $@
	@echo "Boot1: sector 1"
	@echo "Boot2: sectors 2-3"
	@echo "Kernel: sector 4+"
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
