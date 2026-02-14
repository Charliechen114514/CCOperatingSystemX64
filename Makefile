# Makefile for CCOS Bootloader (Two-Stage)

AS = nasm
ASFLAGS = -f bin
QEMU = qemu-system-x86_64

# Directories
SRC_DIR = boot
BUILD_DIR = build

# Source files
BOOT_ASM = $(SRC_DIR)/boot.asm
BOOT2_ASM = $(SRC_DIR)/boot2.asm

# Output files in build directory
BOOT_BIN = $(BUILD_DIR)/boot.bin
BOOT2_BIN = $(BUILD_DIR)/boot2.bin
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

# Combine both stages into boot image
# MBR is 512 bytes, stage 2 starts at sector 2 (offset 512)
$(BOOT_IMG): $(BOOT_BIN) $(BOOT2_BIN)
	@echo "Combining stages into boot image..."
	dd if=$(BOOT_BIN) of=$@ bs=512 count=1 conv=notrunc 2>/dev/null
	dd if=$(BOOT2_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "Final boot image:"
	@ls -lh $@
	@echo "Done! Boot image: $@"

clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)
	@rm -f serial.log
	@echo "Clean done."

run: $(BOOT_IMG)
	@echo "Running with QEMU..."
	$(QEMU) -drive format=raw,file=$(BOOT_IMG),if=floppy -nographic

debug: $(BOOT_IMG)
	@echo "Running with QEMU (debug)..."
	$(QEMU) -drive format=raw,file=$(BOOT_IMG),if=floppy -nographic -s -S
