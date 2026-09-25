#include <asm/trap_layout.h>

.section .text
.globl arch_trap_entry
.balign 4
arch_trap_entry:
  lpad 0
  // Swap sp with sscratch to determine privilege mode.
  // In S-mode kernel execution, sscratch is 0.
  // When running in U-mode, sscratch contains the kernel stack pointer.
  csrrw sp, sscratch, sp
  bnez sp, .Lfrom_user

  // Trap from Supervisor mode:
  // sp is 0, sscratch holds the original supervisor sp.
  csrrw sp, sscratch, sp          // Restore supervisor sp; sscratch is 0 again
  addi sp, sp, -TF_SIZE           // Allocate trap frame on supervisor stack
  sd s0, TF_S0(sp)                // Save s0 temporarily
  addi s0, sp, TF_SIZE            // Compute original supervisor sp
  sd s0, TF_SP(sp)                // Save original sp in trap frame
  j .Lsave_context

.Lfrom_user:
  // Trap from User mode:
  // sp is the kernel stack, sscratch is the user sp.
  addi sp, sp, -TF_SIZE           // Allocate trap frame on kernel stack
  sd s0, TF_S0(sp)                // Save s0 temporarily
  csrr s0, sscratch               // Retrieve user sp
  sd s0, TF_SP(sp)                // Save original user sp in trap frame
  csrw sscratch, zero             // In kernel mode, sscratch is 0
  sd tp, TF_TP(sp)                // Save user tp in trap frame
  ld tp, TF_KERNEL_TP(sp)         // Restore kernel CPU identity in tp
  j .Lsave_context_rest

.Lsave_context:
  sd tp, TF_TP(sp)
.Lsave_context_rest:
  sd ra, TF_RA(sp)
  sd gp, TF_GP(sp)
  sd t0, TF_T0(sp)
  sd t1, TF_T1(sp)
  sd t2, TF_T2(sp)
  // s0 already saved at TF_S0(sp)
  sd s1, TF_S1(sp)
  sd a0, TF_A0(sp)
  sd a1, TF_A1(sp)
  sd a2, TF_A2(sp)
  sd a3, TF_A3(sp)
  sd a4, TF_A4(sp)
  sd a5, TF_A5(sp)
  sd a6, TF_A6(sp)
  sd a7, TF_A7(sp)
  sd s2, TF_S2(sp)
  sd s3, TF_S3(sp)
  sd s4, TF_S4(sp)
  sd s5, TF_S5(sp)
  sd s6, TF_S6(sp)
  sd s7, TF_S7(sp)
  sd s8, TF_S8(sp)
  sd s9, TF_S9(sp)
  sd s10, TF_S10(sp)
  sd s11, TF_S11(sp)
  sd t3, TF_T3(sp)
  sd t4, TF_T4(sp)
  sd t5, TF_T5(sp)
  sd t6, TF_T6(sp)

  csrr t0, sstatus
  csrr t1, sepc
  csrr t2, scause
  csrr t3, stval

  sd t0, TF_SSTATUS(sp)
  sd t1, TF_SEPC(sp)
  sd t2, TF_SCAUSE(sp)
  sd t3, TF_STVAL(sp)

  // Invoke C trap dispatcher with pointer to struct trap_frame in a0
  mv a0, sp
  call arch_trap_handler

.Lrestore_context:
  // Restore CSRs (dispatcher may have updated sepc or sstatus)
  ld t0, TF_SSTATUS(sp)
  ld t1, TF_SEPC(sp)
  csrw sstatus, t0
  csrw sepc, t1

  // Check if returning to User mode: sstatus.SPP (bit 8) == 0
  andi t2, t0, (1 << 8)
  bnez t2, .Lrestore_supervisor

  // Returning to User mode:
  // sscratch must be reset to the top of the kernel stack for the next trap
  addi t2, sp, TF_SIZE
  csrw sscratch, t2

  // Restore user context
  ld ra, TF_RA(sp)
  ld gp, TF_GP(sp)
  ld tp, TF_TP(sp)
  ld t0, TF_T0(sp)
  ld t1, TF_T1(sp)
  ld t2, TF_T2(sp)
  ld s0, TF_S0(sp)
  ld s1, TF_S1(sp)
  ld a0, TF_A0(sp)
  ld a1, TF_A1(sp)
  ld a2, TF_A2(sp)
  ld a3, TF_A3(sp)
  ld a4, TF_A4(sp)
  ld a5, TF_A5(sp)
  ld a6, TF_A6(sp)
  ld a7, TF_A7(sp)
  ld s2, TF_S2(sp)
  ld s3, TF_S3(sp)
  ld s4, TF_S4(sp)
  ld s5, TF_S5(sp)
  ld s6, TF_S6(sp)
  ld s7, TF_S7(sp)
  ld s8, TF_S8(sp)
  ld s9, TF_S9(sp)
  ld s10, TF_S10(sp)
  ld s11, TF_S11(sp)
  ld t3, TF_T3(sp)
  ld t4, TF_T4(sp)
  ld t5, TF_T5(sp)
  ld t6, TF_T6(sp)
  ld sp, TF_SP(sp)                    // Restore user sp from frame
  sret

.Lrestore_supervisor:
  // Restore supervisor context
  ld ra, TF_RA(sp)
  ld gp, TF_GP(sp)
  ld tp, TF_TP(sp)
  ld t0, TF_T0(sp)
  ld t1, TF_T1(sp)
  ld t2, TF_T2(sp)
  ld s0, TF_S0(sp)
  ld s1, TF_S1(sp)
  ld a0, TF_A0(sp)
  ld a1, TF_A1(sp)
  ld a2, TF_A2(sp)
  ld a3, TF_A3(sp)
  ld a4, TF_A4(sp)
  ld a5, TF_A5(sp)
  ld a6, TF_A6(sp)
  ld a7, TF_A7(sp)
  ld s2, TF_S2(sp)
  ld s3, TF_S3(sp)
  ld s4, TF_S4(sp)
  ld s5, TF_S5(sp)
  ld s6, TF_S6(sp)
  ld s7, TF_S7(sp)
  ld s8, TF_S8(sp)
  ld s9, TF_S9(sp)
  ld s10, TF_S10(sp)
  ld s11, TF_S11(sp)
  ld t3, TF_T3(sp)
  ld t4, TF_T4(sp)
  ld t5, TF_T5(sp)
  ld t6, TF_T6(sp)
  addi sp, sp, TF_SIZE                // Deallocate trap frame
  sret
