#pragma once

#include <stddef.h>
#include <stdint.h>

struct boot_allocator {
  uintptr_t current;
  uintptr_t end;
};

// Linker-defined bounds for the early kernel bootstrap arena.
// These need to be defined by the linker script for each architecture.
extern char boot_allocator_start[];
extern char boot_allocator_end[];

void* boot_alloc(struct boot_allocator* allocator, size_t size,
                 size_t alignment);
