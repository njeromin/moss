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
    .boot : { KEEP(*(.init)) *(.boot.rodata) } :boot
    .boot.bss (NOLOAD) : ALIGN(4096) { *(.boot.bss) } :boot
    ASSERT(. <= KERNEL_LMA, "Bootstrap overlaps kernel physical load address")

    . = KERNEL_VMA;
    .text : AT(KERNEL_LMA) {
        KEEP(*(.text.entry))
        *(.text .text.*)
    } :text
    .rodata ALIGN(4096) : AT(ADDR(.rodata) - KERNEL_VMA + KERNEL_LMA) {
        *(.rodata .rodata.* .srodata .srodata.*)
    } :rodata
    .data ALIGN(4096) : AT(ADDR(.data) - KERNEL_VMA + KERNEL_LMA) {
        *(.data .data.*)
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.sdata .sdata.*)
    } :data
    .bss ALIGN(4096) (NOLOAD) : AT(ADDR(.bss) - KERNEL_VMA + KERNEL_LMA) {
        bss_start = .;
        *(.bss .bss.* .sbss .sbss.*)
        *(COMMON)
        . = ALIGN(16);
        bss_end = .;
        . += 16384;
        stack_top = .;
    } :data
    kernel_end = .;
    ASSERT(kernel_end - KERNEL_VMA <= 0x200000,
           "Kernel exceeds initial 2 MiB higher-half mapping")
    /DISCARD/ : { *(.eh_frame .eh_frame.*) }
}
