# Makefile for CCOS Bootloader + Kernel

AS = nasm
ASFLAGS = -f bin
ASFLAGS_ELF = -f elf64
CC = gcc
CFLAGS = -m64 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -mcmodel=large -ffreestanding
LD = ld
LDFLAGS = -m elf_x86_64 -T linker.ld -nostdlib
QEMU = qemu-system-x86_64

# Directories
SRC_DIR = boot
KERNEL_DIR = kernel
BUILD_DIR = build

# Source files
BOOTLOADER_ASM = $(SRC_DIR)/bootloader.asm
KERNEL_ENTRY_ASM = $(KERNEL_DIR)/kernel_entry.asm
KERNEL_MAIN_C = $(KERNEL_DIR)/kernel_main.c

# Output files in build directory
BOOTLOADER_BIN = $(BUILD_DIR)/bootloader.bin
KERNEL_ENTRY_OBJ = $(BUILD_DIR)/kernel_entry.o
KERNEL_MAIN_OBJ = $(BUILD_DIR)/kernel_main.o
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
BOOT_IMG = $(BUILD_DIR)/boot.img

.PHONY: all clean run debug prepare

all: $(BOOT_IMG)

# Prepare build directory
prepare:
	@mkdir -p $(BUILD_DIR)

# Build unified bootloader (contains both stage 1 and stage 2)
$(BOOTLOADER_BIN): $(BOOTLOADER_ASM) | prepare
	@echo "Building unified bootloader..."
	$(AS) $(ASFLAGS) $< -o $@
	@echo "Bootloader size:"
	@ls -lh $@

# Build kernel entry (assembly) - ELF format for linking
$(KERNEL_ENTRY_OBJ): $(KERNEL_ENTRY_ASM) | prepare
	@echo "Building kernel entry..."
	$(AS) $(ASFLAGS_ELF) $< -o $@

# Build kernel main (C) - ELF format for linking
$(KERNEL_MAIN_OBJ): $(KERNEL_MAIN_C) | prepare
	@echo "Building kernel main..."
	$(CC) $(CFLAGS) -c $< -o $@

# Link kernel objects and extract raw binary
$(KERNEL_BIN): $(KERNEL_ENTRY_OBJ) $(KERNEL_MAIN_OBJ) linker.ld | prepare
	@echo "Linking kernel..."
	$(LD) $(LDFLAGS) -o $(BUILD_DIR)/kernel.elf $(KERNEL_ENTRY_OBJ) $(KERNEL_MAIN_OBJ)
	@echo "Extracting kernel binary..."
	@objcopy -O binary $(BUILD_DIR)/kernel.elf $@
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
