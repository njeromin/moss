// Test-only secondary entry. Its physical page is identity-mapped in test_root.
.section .init
.balign 16
.global paging_smp_entry
paging_smp_entry:
  csrci sstatus, 2
  csrw sie, zero
  csrw sscratch, zero
  ld t0, 0(a1)                 // satp
  ld sp, 8(a1)                 // high stack
  ld t1, 16(a1)                // high C entry
  csrw satp, t0
  sfence.vma
  jr t1

.section .rodata
.balign 8
.global paging_smp_entry_address
paging_smp_entry_address:
  .quad paging_smp_entry
