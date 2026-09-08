#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Initialise architecture and CPU features for the specified hart (e.g. Svadu). */
void arch_cpu_init(size_t hart_id);

/* Returns true if hardware PTE Accessed/Dirty bit updating (Svadu) is enabled. */
bool arch_has_svadu(void);
