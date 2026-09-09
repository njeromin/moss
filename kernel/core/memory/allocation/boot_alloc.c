#include <core/memory/allocation/boot_alloc.h>

#include <common/memory.h>
#include <core/memory/map.h>
#include <core/panic.h>

static int allocations_enabled = 0;

void boot_alloc_init(void) {
  const struct memory_map* map = memory_map_get();
  if (map == NULL || map->available_count == 0) {
    kpanic("Invalid memory map for boot allocator.\n");
    return;
  }

  for (size_t i = 0; i < map->available_count; ++i) {
    if (map->available[i].size > UINTPTR_MAX - map->available[i].offset) {
      kpanic("Overflowing available boot memory region.\n");
    }
  }
  for (size_t i = 0; i < map->reserved_count; ++i) {
    if (map->reserved[i].size > UINTPTR_MAX - map->reserved[i].offset) {
      kpanic("Overflowing reserved boot memory region.\n");
    }
  }

  allocations_enabled = 1;
}

const struct memory_map* boot_alloc_memory_map(void) {
  return memory_map_get();
}

void boot_alloc_finish(void) {
  allocations_enabled = 0;
}

void* boot_alloc(size_t size, size_t alignment) {
  if (!allocations_enabled || size == 0 || alignment == 0 ||
      (alignment & (alignment - 1)) != 0) {
    return NULL;
  }

  const struct memory_map* map = memory_map_get();

  for (size_t i = 0; i < map->available_count; ++i) {
    struct memory_region region = map->available[i];
    if (region.flags & (MEMORY_REGION_RESERVED | MEMORY_REGION_NO_MAP)) {
      continue;
    }
    uintptr_t start = region.offset;
    uintptr_t end = start + region.size;
    // A null pointer cannot represent a successful allocation.
    if (start == 0) {
      start = 1;
    }

    while (start < end && start <= UINTPTR_MAX - (alignment - 1)) {
      uintptr_t candidate = align_up(start, alignment);
      if (candidate >= end || size > end - candidate) {
        break;
      }
      uintptr_t candidate_end = candidate + size;
      size_t j;
      for (j = 0; j < map->reserved_count; ++j) {
        struct memory_region reservation = map->reserved[j];
        uintptr_t reserved_end = reservation.offset + reservation.size;
        if (reservation.size != 0 && candidate < reserved_end &&
            reservation.offset < candidate_end) {
          start = reserved_end;
          break;
        }
      }
      if (j != map->reserved_count) {
        continue;
      }

      struct memory_region new_reservation = {
          .offset = candidate,
          .size = size,
          .flags = MEMORY_REGION_RESERVED,
      };

      if (memory_map_add_reserved(new_reservation) != 0) {
        return NULL;
      }

      return (void*)candidate;
    }
  }
  return NULL;
}
