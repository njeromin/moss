#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <asm/cpu.h>

enum cpu_feature {
  CPU_FEATURE_FPU,
  CPU_FEATURE_VECTOR,
  CPU_FEATURE_HW_PAGING_AD,
  CPU_FEATURE_LANDING_PAD,
  CPU_FEATURE_SHADOW_STACK,
  CPU_FEATURE_DOUBLE_TRAP,
  CPU_FEATURE_TIMER_DIRECT,
  CPU_FEATURE_HART_SUSPEND,
  CPU_FEATURE_UNALIGNED_ACCESS,
  CPU_FEATURE_MAX,
};

// Initialise architecture features and trap handling on the executing CPU.
void arch_cpu_init(cpu_hardware_id_t hardware_id);

// Query whether a specific architecture CPU feature is available and enabled.
bool arch_cpu_has_feature(enum cpu_feature feature);

// Enable interrupts globally on the current CPU core (sstatus.SIE).
void arch_interrupts_enable(void);
// Disable interrupts globally on the current CPU core (sstatus.SIE).
void arch_interrupts_disable(void);
// Check if interrupts are globally enabled on the current CPU core (sstatus.SIE).
bool arch_interrupts_enabled(void);

// Enter low-power idle state until an interrupt occurs.
void arch_cpu_idle(void);
