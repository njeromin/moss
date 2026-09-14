.section .init
.balign 4096
.global secondary_entry
secondary_entry:
  csrci sstatus, 2
  csrw sie, zero
  csrw sscratch, zero
  fence r, rw
  // Private startup block: satp, stack, high C entry, logical ID.
  ld t0, 0(a1)
  ld sp, 8(a1)
  ld t1, 16(a1)
  ld tp, 24(a1)
  csrw satp, t0
  sfence.vma
  fence.i
  jr t1
.balign 4096

.section .rodata
.balign 8
.global secondary_entry_address
secondary_entry_address:
  .quad secondary_entry
