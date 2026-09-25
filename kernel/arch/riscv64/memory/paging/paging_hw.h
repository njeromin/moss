#pragma once
#include <arch/memory/paging.h>

// Caller holds the paging lock with supervisor interrupts disabled.
void riscv64_paging_activate(phys_addr_t root, size_t tag);
