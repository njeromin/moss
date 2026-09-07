#pragma once

#include "arch/memory/layout.h"
#include <stddef.h>
#include <stdint.h>

// TODO: Ensure this is the same size as the smallest page in the current architecture.
#define PMM_FRAME_SIZE 4096

struct pmm_region {
  uintptr_t base;
  size_t frame_count;
  uint8_t* bitmap;
};

struct pmm_ctx {
  struct pmm_region* regions;
  size_t region_count;
  size_t region_capacity;
  struct boot_allocator* boot_allocator;
};

int pmm_init(const struct memory_map* memory_map,
             struct boot_allocator* boot_allocator);

int pmm_alloc(size_t frames, uintptr_t* out_ptr);
int pmm_free(uintptr_t base, size_t frames);
