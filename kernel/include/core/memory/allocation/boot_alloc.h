#pragma once

#include <arch/memory/layout.h>
#include <stddef.h>
#include <stdint.h>

// Initialise the boot allocator. Invalid memory map panics.
void boot_alloc_init(void);

// Reserve bytes in available RAM; alignment must be a power of two.
// Returns NULL on exhaustion, invalid arguments, or after the PMM handoff.
void* boot_alloc(size_t size, size_t alignment);

// Includes all allocations made since initialisation.
const struct memory_map* boot_alloc_memory_map(void);
// Disable early allocations before the PMM consumes the final reservations.
void boot_alloc_finish(void);
