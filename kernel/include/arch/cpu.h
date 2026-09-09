#pragma once

#include <stdbool.h>
#include <stddef.h>

// Initialise architecture and CPU features for the specified hart (e.g. Svadu).
void arch_cpu_init(size_t hart_id);

// Yield processor resources and reduces power consumption in spin loops.
void arch_cpu_relax(void);

// Enable interrupts globally on the current CPU core (sstatus.SIE).
void arch_interrupts_enable(void);
// Disable interrupts globally on the current CPU core (sstatus.SIE).
void arch_interrupts_disable(void);
// Check if interrupts are globally enabled on the current CPU core (sstatus.SIE).
bool arch_interrupts_enabled(void);
