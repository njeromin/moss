#include <asm/trap_layout.h>

.section .text, "ax", @progbits
.globl arch_user_enter
.type arch_user_enter, @function
.balign 4
arch_user_enter:
  lpad 0
  // a0 holds pointer to struct trap_frame on top of current thread's kernel stack.
  mv sp, a0

  // Preserve Kernel CPU logical ID (tp) in trap frame for restoration upon trap
  sd tp, TF_KERNEL_TP(sp)

  // Restore CSRs: sstatus and sepc
  ld t0, TF_SSTATUS(sp)
  ld t1, TF_SEPC(sp)
  csrw sstatus, t0
  csrw sepc, t1

  // Set sscratch to the top of the kernel stack (sp + TF_SIZE) for subsequent traps
  addi t2, sp, TF_SIZE
  csrw sscratch, t2

  // Restore user general-purpose registers
  ld ra, TF_RA(sp)
  ld gp, TF_GP(sp)
  ld tp, TF_TP(sp)
  ld t0, TF_T0(sp)
  ld t1, TF_T1(sp)
  ld t2, TF_T2(sp)
  ld s0, TF_S0(sp)
  ld s1, TF_S1(sp)
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

  // Restore a0 and user stack pointer last
  ld a0, TF_A0(sp)
  ld sp, TF_SP(sp)

  sret
.size arch_user_enter, . - arch_user_enter
