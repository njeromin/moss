#pragma once

#include <stddef.h>
#include <stdint.h>

enum memory_region_flags {
  MEMORY_REGION_RESERVED = 1u << 0,
  MEMORY_REGION_NO_MAP = 1u << 1,
  MEMORY_REGION_REUSABLE = 1u << 2,
  MEMORY_REGION_FIRMWARE = 1u << 3,
};

struct memory_region {
  // Offset of this memory region.
  uintptr_t offset;
  // Size of this memory region.
  size_t size;
  // Flags using `memory_region_flags`.
  uint8_t flags;
};
