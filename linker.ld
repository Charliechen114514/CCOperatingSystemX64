/* CCOS Kernel Linker Script */
/* Kernel is loaded at 0x10000 by bootloader */
/*
 * IMPORTANT: All sections must be contiguous without gaps for objcopy -O binary
 * to work correctly. The bootloader loads the kernel binary to physical 0x10000,
 * and the kernel uses identity mapping to access it.
 */

ENTRY(kernel_start)

SECTIONS
{
    /* Kernel starts at 0x10000 */
    . = 0x10000;

    /* Text section - code */
    .text : {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(8);
    }

    /* Initialized data - immediately after .rodata */
    .data : {
        *(.data)
        *(.data.*)
        . = ALIGN(8);
    }

    /* Uninitialized data - regular BSS */
    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    }

    /* Large BSS - for large uninitialized data (e.g., page frame bitmap) */
    .lbss : {
        __lbss_start = .;
        *(.lbss)
        *(.lbss.*)
        . = ALIGN(8);
        __lbss_end = .;
    }

    /* Discard sections we don't need */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
