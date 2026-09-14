OUTPUT_ARCH(riscv)
ENTRY(_start)

PHDRS {
    text PT_LOAD FLAGS(5);   /* Read + Execute */
    rodata PT_LOAD FLAGS(4); /* Read-only */
    data PT_LOAD FLAGS(6);   /* Read + Write */
}

SECTIONS {
    . = 0x00010000;

    .text : {
        text_start = .;
        KEEP(*(.text.entry))
        KEEP(*(.text._start))
        *(.text .text.*)
        . = ALIGN(4096);
        text_end = .;
    } :text

    .rodata ALIGN(4096) : {
        rodata_start = .;
        *(.rodata .rodata.* .srodata .srodata.*)
        . = ALIGN(4096);
        rodata_end = .;
    } :rodata

    .data ALIGN(4096) : {
        data_start = .;
        *(.data .data.*)
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.sdata .sdata.*)
        . = ALIGN(4096);
        data_end = .;
    } :data

    .bss ALIGN(4096) (NOLOAD) : {
        . = ALIGN(8);
        bss_start = .;
        *(.bss .bss.* .sbss .sbss.*)
        *(COMMON)
        . = ALIGN(8);
        bss_end = .;
        . = ALIGN(4096);
    } :data

    /DISCARD/ : {
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
        *(.comment)
    }
}
