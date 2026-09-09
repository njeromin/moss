OUTPUT_ARCH(riscv)
ENTRY(start)
KERNEL_VMA = 0xffffffff80000000;
KERNEL_LMA = 0x80400000;

PHDRS {
    boot PT_LOAD FLAGS(7);
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

SECTIONS {
    /* OpenSBI occupies the beginning of RAM. Bootstrap runs without paging. */
    . = 0x80200000;
    bootstrap_start = .;
    .boot : { KEEP(*(.init)) *(.boot.rodata) } :boot
    .boot.bss (NOLOAD) : ALIGN(4096) { *(.boot.bss) } :boot
    bootstrap_end = .;
    ASSERT(. <= KERNEL_LMA, "Bootstrap overlaps kernel physical load address")

    . = KERNEL_VMA;
    .text : AT(KERNEL_LMA) {
        text_start = .;
        kernel_text_start = .;
        KEEP(*(.text.entry))
        *(.text .text.*)
        . = ALIGN(4096);
        text_end = .;
        kernel_text_end = .;
    } :text
    .rodata ALIGN(4096) : AT(ADDR(.rodata) - KERNEL_VMA + KERNEL_LMA) {
        rodata_start = .;
        kernel_rodata_start = .;
        /* Store values in high memory so medany C can read the physical base. */
        kernel_virtual_base = .;
        QUAD(KERNEL_VMA)
        kernel_physical_base = .;
        QUAD(KERNEL_LMA)
        bootstrap_physical_start = .;
        QUAD(bootstrap_start)
        bootstrap_physical_end = .;
        QUAD(bootstrap_end)
        kernel_physical_start = .;
        QUAD(KERNEL_LMA)
        *(.rodata .rodata.* .srodata .srodata.*)
        . = ALIGN(4096);
        rodata_end = .;
        kernel_rodata_end = .;
    } :rodata
    .data ALIGN(4096) : AT(ADDR(.data) - KERNEL_VMA + KERNEL_LMA) {
        data_start = .;
        kernel_data_start = .;
        *(.data .data.*)
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.sdata .sdata.*)
        kernel_physical_end = .;
        QUAD(kernel_end - KERNEL_VMA + KERNEL_LMA)
        . = ALIGN(4096);
        data_end = .;
        kernel_data_end = .;
    } :data
    .bss ALIGN(4096) (NOLOAD) : AT(ADDR(.bss) - KERNEL_VMA + KERNEL_LMA) {
        bss_start = .;
        kernel_bss_start = .;
        *(.bss .bss.* .sbss .sbss.*)
        *(COMMON)
        . = ALIGN(16);
        bss_end = .;
        kernel_bss_end = .;
        . += 16384;
        . = ALIGN(16);
        stack_top = .;
        . = ALIGN(4096);
    } :data
    kernel_end = .;
    ASSERT(kernel_end - KERNEL_VMA <= 0x200000,
           "Kernel exceeds initial 2 MiB higher-half mapping")
    /DISCARD/ : { *(.eh_frame .eh_frame.*) }
}
