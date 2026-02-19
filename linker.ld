/* CCOS Kernel Linker Script */
/* Kernel is loaded at 0x10000 by bootloader */

ENTRY(kernel_start)

SECTIONS
{
    /* Kernel starts at 0x10000 */
    . = 0x10000;

    /* Text section - code */
    .text : {
        *(.text)
        *(.text.*)
    }

    /* Read-only data */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    /* Initialized data */
    .data : {
        *(.data)
        *(.data.*)
    }

    /* Uninitialized data - regular BSS */
    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* Large BSS section for big static arrays (e.g., 2MB bitmap)
     * This section is placed after regular BSS and uses NOLOAD attribute
     * to prevent it from being included in the binary file.
     * The bootloader must ensure enough memory is available for this section.
     */
    .lbss (NOLOAD) : {
        __lbss_start = .;
        *(.lbss)
        *(.lbss.*)
        __lbss_end = .;
    }

    /* Discard sections we don't need */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
