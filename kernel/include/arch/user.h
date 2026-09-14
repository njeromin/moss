#pragma once

#include <stdint.h>
#include <asm/trap_layout.h>

// Initialise a trap frame for first-time usermode execution.
// Sets entry PC, user SP, initial argument, user privilege flags, and zeroes secrets.
void arch_user_context_init(struct trap_frame* tf, uintptr_t entry_point,
                            uintptr_t user_sp, uintptr_t arg0);

// Set or update the thread pointer / TLS base in the given trap frame.
void arch_user_context_set_tls(struct trap_frame* tf, uintptr_t tls_base);

// Enter user mode by restoring the supplied trap frame on the current CPU.
// tf must reside at the top of the current thread's kernel stack.
// This function never returns.
_Noreturn void arch_user_enter(struct trap_frame* tf);
