#pragma once

#include <arch/platform/trap_layout.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Initialise trap handling on the current hart (sets stvec and sscratch).
void arch_trap_init(void);

// Low-level assembly entry point for supervisor traps.
void arch_trap_entry(void);

// C dispatcher called from assembly trap entry.
void arch_trap_handler(struct trap_frame* tf);
