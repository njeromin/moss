.equ PAGE_SHIFT,          12
.equ PAGE_SIZE,           (1 << PAGE_SHIFT)
.equ PTE_SIZE,            8
.equ PTE_PPN_SHIFT,       10

.equ PTE_VALID,           (1 << 0)
.equ PTE_READ,            (1 << 1)
.equ PTE_WRITE,           (1 << 2)
.equ PTE_EXECUTE,         (1 << 3)
.equ PTE_ACCESSED,        (1 << 6)
.equ PTE_DIRTY,           (1 << 7)
// Temporary supervisor-only RWX mappings; A/D avoid early page faults.
.equ BOOT_LEAF_FLAGS,     (PTE_VALID | PTE_READ | PTE_WRITE | PTE_EXECUTE | PTE_ACCESSED | PTE_DIRTY)

.equ RAM_BASE,            0x80000000
.equ IDENTITY_ROOT_SLOT,  2       // VA 0x80000000, a 1 GiB identity mapping.
.equ KERNEL_ROOT_SLOT,    510     // VA 0xffffffff80000000; matches KERNEL_VMA.
.equ SATP_SV39,           (8 << 60)
.equ SSTATUS_SIE,         (1 << 1)

.section .init, "ax", @progbits
.option norvc
.option norelax
.cfi_sections .debug_frame

.global start
.type start, @function
start:
  // OpenSBI boot hart only: S-mode, satp=0, a0=hart ID, a1=physical DTB.
  // Secondary harts need a separate entry and stack.
  // Keep interrupts disabled until the kernel installs a real trap handler.
  csrci sstatus, SSTATUS_SIE
  csrw sie, zero
  la t0, .Lboot_halt
  csrw stvec, t0

  // Clear page tables explicitly rather than relying on the ELF loader.
  la t0, boot_root
  la t1, boot_tables_end
.Lclear_boot_tables:
  sd zero, 0(t0)
  addi t0, t0, PTE_SIZE
  bltu t0, t1, .Lclear_boot_tables

  // From here: t0 = root table, t1 = L1 table, t2 = PTE, t3 = slot address.
  la t0, boot_root
  // Root[2]: identity map 0x80000000..0xbfffffff with a 1 GiB leaf.
  // V|R|W|X|A|D, supervisor only. Retain for physical DTB access for now.
  li t1, ((RAM_BASE >> PAGE_SHIFT) << PTE_PPN_SHIFT) | BOOT_LEAF_FLAGS
  sd t1, (IDENTITY_ROOT_SLOT * PTE_SIZE)(t0)

  // Root[510]: high region starting at 0xffffffff80000000.
  la t1, boot_kernel_l1
  // Non-leaf PTE: physical page number plus V, without R/W/X permissions.
  srli t2, t1, PAGE_SHIFT
  slli t2, t2, PTE_PPN_SHIFT
  ori t2, t2, PTE_VALID
  li t3, KERNEL_ROOT_SLOT * PTE_SIZE
  add t3, t0, t3
  sd t2, 0(t3)

  // L1[0]: first 2 MiB of the high kernel maps to KERNEL_LMA.
  la t2, .Lkernel_lma
  ld t2, 0(t2)
  srli t2, t2, PAGE_SHIFT
  slli t2, t2, PTE_PPN_SHIFT
  ori t2, t2, BOOT_LEAF_FLAGS
  sd t2, 0(t1)

  // Absolute destination avoids an out-of-range PC-relative reference.
  la t1, .Lhigh_entry
  ld t1, 0(t1)
  // satp = Sv39 mode | root physical page number (ASID remains zero).
  srli t0, t0, PAGE_SHIFT
  li t2, SATP_SV39
  or t0, t0, t2
  sfence.vma zero, zero
  csrw satp, t0
  sfence.vma zero, zero
  jr t1

.balign 4
.Lboot_halt:
  wfi
  j .Lboot_halt
.size start, . - start

.section .boot.rodata, "a", @progbits
.balign 8
.Lhigh_entry:
  .dword higher_half_entry
.Lkernel_lma:
  .dword KERNEL_LMA

.section .boot.bss, "aw", @nobits
.balign PAGE_SIZE
boot_root:
  .skip PAGE_SIZE
boot_kernel_l1:
  .skip PAGE_SIZE
boot_tables_end:

.section .text.entry, "ax", @progbits
.global higher_half_entry
.type higher_half_entry, @function
higher_half_entry:
  .cfi_startproc
  .cfi_undefined ra

  // All symbol references below use high virtual addresses.
  la t0, .Lhalt
  csrw stvec, t0
  la gp, __global_pointer$

  // The linker aligns both BSS boundaries to allow 8-byte stores.
  la t0, bss_start
  la t1, bss_end
.Lclear_kernel_bss:
  bgeu t0, t1, .Lenter_kernel_c
  sd zero, 0(t0)
  addi t0, t0, 8
  j .Lclear_kernel_bss

.Lenter_kernel_c:
  // a0/a1 still hold OpenSBI's hart ID and physical DTB address.
  la sp, stack_top
  call arch_startup_core_entry

  // Park if C returns. It may have enabled interrupts in the meantime.
  csrci sstatus, SSTATUS_SIE
  csrw sie, zero
.balign 4
.Lhalt:
  wfi
  j .Lhalt
  .cfi_endproc
.size higher_half_entry, . - higher_half_entry

.section .note.GNU-stack, "", @progbits
