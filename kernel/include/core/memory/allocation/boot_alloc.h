#pragma once

#include <arch/memory/layout.h>
#include <stddef.h>
#include <stdint.h>

// Copy the discovery map into owned storage. Invalid input panics.
void boot_alloc_init(struct memory_map* memory_map);

// Reserve bytes in available RAM; alignment must be a power of two.
// Returns NULL on exhaustion, invalid arguments, or after the PMM handoff.
void* boot_alloc(size_t size, size_t alignment);

// Includes all allocations made since initialization.
const struct memory_map* boot_alloc_memory_map(void);
// Disable early allocations before the PMM consumes the final reservations.
void boot_alloc_finish(void);
