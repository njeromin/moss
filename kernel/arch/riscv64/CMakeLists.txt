list(APPEND KERNEL_SOURCES
    arch/riscv64/entry.c
    arch/riscv64/entry.S
    arch/riscv64/sbi.c
    arch/riscv64/panic.c
    arch/riscv64/memory/paging.c
    arch/riscv64/memory/layout/layout.c
    arch/riscv64/memory/layout/memory_map.c
    arch/riscv64/tempconsole.c
)
