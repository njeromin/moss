.section .text
.globl arch_trap_entry
.balign 4
arch_trap_entry:
  // Swap sp with sscratch to determine privilege mode.
  // In S-mode kernel execution, sscratch is 0.
  // When running in U-mode, sscratch contains the kernel stack pointer.
  csrrw sp, sscratch, sp
  bnez sp, .Lfrom_user

  // Trap from Supervisor mode:
  // sp is 0, sscratch holds the original supervisor sp.
  csrrw sp, sscratch, sp          // Restore supervisor sp; sscratch is 0 again
  addi sp, sp, -288               // Allocate trap frame on supervisor stack
  sd s0, 56(sp)                   // Save s0 temporarily
  addi s0, sp, 288                // Compute original supervisor sp
  sd s0, 8(sp)                    // Save original sp in trap frame
  j .Lsave_context

.Lfrom_user:
  // Trap from User mode:
  // sp is the kernel stack, sscratch is the user sp.
  addi sp, sp, -288               // Allocate trap frame on kernel stack
  sd s0, 56(sp)                   // Save s0 temporarily
  csrr s0, sscratch               // Retrieve user sp
  sd s0, 8(sp)                    // Save original user sp in trap frame
  csrw sscratch, zero             // In kernel mode, sscratch is 0

.Lsave_context:
  sd ra, 0(sp)
  sd gp, 16(sp)
  sd tp, 24(sp)
  sd t0, 32(sp)
  sd t1, 40(sp)
  sd t2, 48(sp)
  // s0 already saved at 56(sp)
  sd s1, 64(sp)
  sd a0, 72(sp)
  sd a1, 80(sp)
  sd a2, 88(sp)
  sd a3, 96(sp)
  sd a4, 104(sp)
  sd a5, 112(sp)
  sd a6, 120(sp)
  sd a7, 128(sp)
  sd s2, 136(sp)
  sd s3, 144(sp)
  sd s4, 152(sp)
  sd s5, 160(sp)
  sd s6, 168(sp)
  sd s7, 176(sp)
  sd s8, 184(sp)
  sd s9, 192(sp)
  sd s10, 200(sp)
  sd s11, 208(sp)
  sd t3, 216(sp)
  sd t4, 224(sp)
  sd t5, 232(sp)
  sd t6, 240(sp)

  csrr t0, sstatus
  csrr t1, sepc
  csrr t2, scause
  csrr t3, stval

  sd t0, 248(sp)
  sd t1, 256(sp)
  sd t2, 264(sp)
  sd t3, 272(sp)

  // Invoke C trap dispatcher with pointer to struct trap_frame in a0
  mv a0, sp
  call arch_trap_handler

  // Restore CSRs (dispatcher may have updated sepc or sstatus)
  ld t0, 248(sp)
  ld t1, 256(sp)
  csrw sstatus, t0
  csrw sepc, t1

  // Check if returning to User mode: sstatus.SPP (bit 8) == 0
  andi t2, t0, (1 << 8)
  bnez t2, .Lrestore_supervisor

  // Returning to User mode:
  // sscratch must be reset to the top of the kernel stack for the next trap
  addi t2, sp, 288
  csrw sscratch, t2

  // Restore user context
  ld ra, 0(sp)
  ld gp, 16(sp)
  ld tp, 24(sp)
  ld t0, 32(sp)
  ld t1, 40(sp)
  ld t2, 48(sp)
  ld s0, 56(sp)
  ld s1, 64(sp)
  ld a0, 72(sp)
  ld a1, 80(sp)
  ld a2, 88(sp)
  ld a3, 96(sp)
  ld a4, 104(sp)
  ld a5, 112(sp)
  ld a6, 120(sp)
  ld a7, 128(sp)
  ld s2, 136(sp)
  ld s3, 144(sp)
  ld s4, 152(sp)
  ld s5, 160(sp)
  ld s6, 168(sp)
  ld s7, 176(sp)
  ld s8, 184(sp)
  ld s9, 192(sp)
  ld s10, 200(sp)
  ld s11, 208(sp)
  ld t3, 216(sp)
  ld t4, 224(sp)
  ld t5, 232(sp)
  ld t6, 240(sp)
  ld sp, 8(sp)                    // Restore user sp from frame
  sret

.Lrestore_supervisor:
  // Restore supervisor context
  ld ra, 0(sp)
  ld gp, 16(sp)
  ld tp, 24(sp)
  ld t0, 32(sp)
  ld t1, 40(sp)
  ld t2, 48(sp)
  ld s0, 56(sp)
  ld s1, 64(sp)
  ld a0, 72(sp)
  ld a1, 80(sp)
  ld a2, 88(sp)
  ld a3, 96(sp)
  ld a4, 104(sp)
  ld a5, 112(sp)
  ld a6, 120(sp)
  ld a7, 128(sp)
  ld s2, 136(sp)
  ld s3, 144(sp)
  ld s4, 152(sp)
  ld s5, 160(sp)
  ld s6, 168(sp)
  ld s7, 176(sp)
  ld s8, 184(sp)
  ld s9, 192(sp)
  ld s10, 200(sp)
  ld s11, 208(sp)
  ld t3, 216(sp)
  ld t4, 224(sp)
  ld t5, 232(sp)
  ld t6, 240(sp)
  addi sp, sp, 288                // Deallocate trap frame
  sret
