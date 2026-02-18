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

    /* User programs embedded as binary data
     * These must be placed after .got.plt and before .bss to maintain
     * consistent memory layout regardless of whether user demos are enabled.
     * The linker will place them here only when ENABLE_USER_DEMO=ON.
     * IMPORTANT: User programs must be 4KB-aligned for proper page mapping.
     */
    . = ALIGN(4096);
    .user_uname_test : {
        *(.user_uname_test)
    }
    /* Catch-all for any future .user_* sections */
    .user (NOLOAD) : {
        *(.user_*)
    }

    /* Uninitialized data */
    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* Discard sections we don't need */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
