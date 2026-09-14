.section .text.entry, "ax", @progbits
.global _start
.type _start, @function

_start:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    // Clear frame pointer and return address
    mv fp, zero
    mv ra, zero

    // Zero the .bss section
    la a1, bss_start
    la a2, bss_end
.Lzero_bss_loop:
    bgeu a1, a2, .Lzero_bss_done
    sd zero, 0(a1)
    addi a1, a1, 8
    j .Lzero_bss_loop
.Lzero_bss_done:

    // Call user main entry
    call main

    // Exit syscall: return status in a0, syscall number in a7
    li a7, 0
    ecall

.Lexit_loop:
    j .Lexit_loop

.size _start, . - _start
