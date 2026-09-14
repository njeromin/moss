#pragma once

#include <asm/cpu.h>
#include <stdbool.h>
#include <stddef.h>

// Initialise architecture features and trap handling on the executing CPU.
void arch_cpu_init(cpu_hardware_id_t hardware_id);

// Enable interrupts globally on the current CPU core (sstatus.SIE).
void arch_interrupts_enable(void);
// Disable interrupts globally on the current CPU core (sstatus.SIE).
void arch_interrupts_disable(void);
// Check if interrupts are globally enabled on the current CPU core (sstatus.SIE).
bool arch_interrupts_enabled(void);
