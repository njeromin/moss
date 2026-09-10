#pragma once

// Yield processor resources and reduce power consumption in spin loops.
static inline void arch_cpu_relax(void) {
  __asm__ volatile(".insn i 0x0F, 0, x0, x0, 0x010" ::: "memory");
}
