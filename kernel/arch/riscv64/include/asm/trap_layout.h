#pragma once

// General Purpose Register (GPR) offsets
#define TF_RA 0
#define TF_SP 8
#define TF_GP 16
#define TF_TP 24
#define TF_T0 32
#define TF_T1 40
#define TF_T2 48
#define TF_S0 56
#define TF_S1 64
#define TF_A0 72
#define TF_A1 80
#define TF_A2 88
#define TF_A3 96
#define TF_A4 104
#define TF_A5 112
#define TF_A6 120
#define TF_A7 128
#define TF_S2 136
#define TF_S3 144
#define TF_S4 152
#define TF_S5 160
#define TF_S6 168
#define TF_S7 176
#define TF_S8 184
#define TF_S9 192
#define TF_S10 200
#define TF_S11 208
#define TF_T3 216
#define TF_T4 224
#define TF_T5 232
#define TF_T6 240

// Supervisor Control and Status Register (CSR) offsets
#define TF_SSTATUS 248
#define TF_SEPC 256
#define TF_SCAUSE 264
#define TF_STVAL 272
#define TF_KERNEL_TP 280

// Total trap frame allocation size on stack (16-byte aligned for RISC-V ABI)
#define TF_SIZE 288

#ifndef __ASSEMBLER__

#include <stddef.h>
#include <stdint.h>

#define TRAP_ARG_REGISTER_COUNT 6

typedef uint64_t trap_register_t;

// Complete processor register state saved during an S-mode trap
struct __attribute__((aligned(16))) trap_frame {
  // General Purpose Registers (GPRs)
  trap_register_t ra;
  trap_register_t sp;
  trap_register_t gp;
  trap_register_t tp;
  trap_register_t t0;
  trap_register_t t1;
  trap_register_t t2;
  trap_register_t s0;  // fp
  trap_register_t s1;
  trap_register_t a0;
  trap_register_t a1;
  trap_register_t a2;
  trap_register_t a3;
  trap_register_t a4;
  trap_register_t a5;
  trap_register_t a6;
  trap_register_t a7;
  trap_register_t s2;
  trap_register_t s3;
  trap_register_t s4;
  trap_register_t s5;
  trap_register_t s6;
  trap_register_t s7;
  trap_register_t s8;
  trap_register_t s9;
  trap_register_t s10;
  trap_register_t s11;
  trap_register_t t3;
  trap_register_t t4;
  trap_register_t t5;
  trap_register_t t6;

  // Supervisor Control and Status Registers (CSRs)
  trap_register_t sstatus;
  trap_register_t sepc;
  trap_register_t scause;
  trap_register_t stval;

  // Preserved Kernel CPU logical ID (tp)
  trap_register_t kernel_tp;
};

_Static_assert(offsetof(struct trap_frame, kernel_tp) == TF_KERNEL_TP,
               "TF_KERNEL_TP offset mismatch");


_Static_assert(offsetof(struct trap_frame, ra) == TF_RA,
               "TF_RA offset mismatch");
_Static_assert(offsetof(struct trap_frame, sp) == TF_SP,
               "TF_SP offset mismatch");
_Static_assert(offsetof(struct trap_frame, gp) == TF_GP,
               "TF_GP offset mismatch");
_Static_assert(offsetof(struct trap_frame, tp) == TF_TP,
               "TF_TP offset mismatch");
_Static_assert(offsetof(struct trap_frame, t0) == TF_T0,
               "TF_T0 offset mismatch");
_Static_assert(offsetof(struct trap_frame, t1) == TF_T1,
               "TF_T1 offset mismatch");
_Static_assert(offsetof(struct trap_frame, t2) == TF_T2,
               "TF_T2 offset mismatch");
_Static_assert(offsetof(struct trap_frame, s0) == TF_S0,
               "TF_S0 offset mismatch");
_Static_assert(offsetof(struct trap_frame, s1) == TF_S1,
               "TF_S1 offset mismatch");
_Static_assert(offsetof(struct trap_frame, a0) == TF_A0,
               "TF_A0 offset mismatch");
_Static_assert(offsetof(struct trap_frame, a1) == TF_A1,
               "TF_A1 offset mismatch");
_Static_assert(offsetof(struct trap_frame, a2) == TF_A2,
               "TF_A2 offset mismatch");
_Static_assert(offsetof(struct trap_frame, a3) == TF_A3,
               "TF_A3 offset mismatch");
_Static_assert(offsetof(struct trap_frame, a4) == TF_A4,
               "TF_A4 offset mismatch");
_Static_assert(offsetof(struct trap_frame, a5) == TF_A5,
               "TF_A5 offset mismatch");
_Static_assert(offsetof(struct trap_frame, a6) == TF_A6,
               "TF_A6 offset mismatch");
_Static_assert(offsetof(struct trap_frame, a7) == TF_A7,
               "TF_A7 offset mismatch");
_Static_assert(offsetof(struct trap_frame, s2) == TF_S2,
               "TF_S2 offset mismatch");
_Static_assert(offsetof(struct trap_frame, s3) == TF_S3,
               "TF_S3 offset mismatch");
_Static_assert(offsetof(struct trap_frame, s4) == TF_S4,
               "TF_S4 offset mismatch");
_Static_assert(offsetof(struct trap_frame, s5) == TF_S5,
               "TF_S5 offset mismatch");
_Static_assert(offsetof(struct trap_frame, s6) == TF_S6,
               "TF_S6 offset mismatch");
_Static_assert(offsetof(struct trap_frame, s7) == TF_S7,
               "TF_S7 offset mismatch");
_Static_assert(offsetof(struct trap_frame, s8) == TF_S8,
               "TF_S8 offset mismatch");
_Static_assert(offsetof(struct trap_frame, s9) == TF_S9,
               "TF_S9 offset mismatch");
_Static_assert(offsetof(struct trap_frame, s10) == TF_S10,
               "TF_S10 offset mismatch");
_Static_assert(offsetof(struct trap_frame, s11) == TF_S11,
               "TF_S11 offset mismatch");
_Static_assert(offsetof(struct trap_frame, t3) == TF_T3,
               "TF_T3 offset mismatch");
_Static_assert(offsetof(struct trap_frame, t4) == TF_T4,
               "TF_T4 offset mismatch");
_Static_assert(offsetof(struct trap_frame, t5) == TF_T5,
               "TF_T5 offset mismatch");
_Static_assert(offsetof(struct trap_frame, t6) == TF_T6,
               "TF_T6 offset mismatch");

_Static_assert(offsetof(struct trap_frame, sstatus) == TF_SSTATUS,
               "TF_SSTATUS offset mismatch");
_Static_assert(offsetof(struct trap_frame, sepc) == TF_SEPC,
               "TF_SEPC offset mismatch");
_Static_assert(offsetof(struct trap_frame, scause) == TF_SCAUSE,
               "TF_SCAUSE offset mismatch");
_Static_assert(offsetof(struct trap_frame, stval) == TF_STVAL,
               "TF_STVAL offset mismatch");

_Static_assert(sizeof(struct trap_frame) == TF_SIZE,
               "Trap frame size must match TF_SIZE");
_Static_assert((TF_SIZE % 16) == 0, "TF_SIZE must be 16-byte aligned");

#endif
