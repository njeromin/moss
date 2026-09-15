#pragma once

#include <asm/trap_layout.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Initialise trap handling on the current hart (sets stvec and sscratch).
void arch_trap_init(void);

// Low-level assembly entry point for supervisor traps.
void arch_trap_entry(void);

// C dispatcher called from assembly trap entry.
void arch_trap_handler(struct trap_frame* tf);

// Returns true if the trap frame originated from usermode execution.
bool arch_trap_is_usermode(const struct trap_frame* tf);
