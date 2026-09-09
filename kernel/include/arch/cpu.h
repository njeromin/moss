#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Initialise architecture and CPU features for the specified hart (e.g. Svadu). */
void arch_cpu_init(size_t hart_id);

/* Returns true if hardware PTE Accessed/Dirty bit updating (Svadu) is enabled. */
bool arch_has_svadu(void);

/* Enable/disable interrupts globally on the current CPU core (sstatus.SIE). */
void arch_interrupts_enable(void);
void arch_interrupts_disable(void);
bool arch_interrupts_enabled(void);
