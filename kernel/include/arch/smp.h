#pragma once

#include <stddef.h>
#include <stdint.h>
#include <arch/cpu.h>

struct cpu_start_info {
  size_t logical_id;
  uintptr_t stack_top;
  void (*entry)(size_t logical_id);
};

// Boot CPU only, serial startup. info is copied before returning.
int arch_smp_start_cpu(cpu_hardware_id_t hardware_id,
                       const struct cpu_start_info* info);
// Called after every started CPU has acknowledged entry into high memory.
void arch_smp_finish_startup(void);

// Kernel-only CPU identity; user trap entry must restore this before using it.
void arch_cpu_set_logical_id(size_t id);
size_t arch_cpu_logical_id(void);

// Send a reschedule IPI to the target hardware CPU core.
void arch_smp_reschedule_cpu(cpu_hardware_id_t hardware_id);
