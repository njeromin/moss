#include <core/memory/allocation/boot_alloc.h>

#include <common/memory.h>
#include <core/panic.h>

#define BOOT_MEMORY_REGION_CAPACITY 64

static struct memory_region available[BOOT_MEMORY_REGION_CAPACITY];
static struct memory_region reserved[BOOT_MEMORY_REGION_CAPACITY];

static struct memory_map boot_memory_map = {
    .available = available,
    .available_capacity = BOOT_MEMORY_REGION_CAPACITY,
    .reserved = reserved,
    .reserved_capacity = BOOT_MEMORY_REGION_CAPACITY,
};

static int allocations_enabled;

void boot_alloc_init(struct memory_map* memory_map) {
  if (memory_map == NULL ||
      memory_map->available_count > memory_map->available_capacity ||
      memory_map->reserved_count > memory_map->reserved_capacity ||
      (memory_map->available_count != 0 && memory_map->available == NULL) ||
      (memory_map->reserved_count != 0 && memory_map->reserved == NULL)) {
    kpanic("Invalid memory map for boot allocator.\n");
    return;
  }
  if (memory_map->available_count > BOOT_MEMORY_REGION_CAPACITY ||
      memory_map->reserved_count > BOOT_MEMORY_REGION_CAPACITY) {
    kpanic("Too many memory regions for boot allocator.\n");
    return;
  }

  for (size_t i = 0; i < memory_map->available_count; ++i) {
    if (memory_map->available[i].size >
        UINTPTR_MAX - memory_map->available[i].offset) {
      kpanic("Overflowing available boot memory region.\n");
    }
  }
  for (size_t i = 0; i < memory_map->reserved_count; ++i) {
    if (memory_map->reserved[i].size >
        UINTPTR_MAX - memory_map->reserved[i].offset) {
      kpanic("Overflowing reserved boot memory region.\n");
    }
  }
  allocations_enabled = 1;

  // Own the region data so the discovery map may live on the caller's stack.
  for (size_t i = 0; i < memory_map->available_count; ++i) {
    available[i] = memory_map->available[i];
  }
  for (size_t i = 0; i < memory_map->reserved_count; ++i) {
    reserved[i] = memory_map->reserved[i];
  }
  boot_memory_map.available_count = memory_map->available_count;
  boot_memory_map.reserved_count = memory_map->reserved_count;
  boot_memory_map.min_phys_addr = memory_map->min_phys_addr;
  boot_memory_map.max_phys_addr = memory_map->max_phys_addr;
}

const struct memory_map* boot_alloc_memory_map(void) {
  return &boot_memory_map;
}

void boot_alloc_finish(void) {
  allocations_enabled = 0;
}

void* boot_alloc(size_t size, size_t alignment) {
  if (!allocations_enabled || size == 0 || alignment == 0 ||
      (alignment & (alignment - 1)) != 0) {
    return NULL;
  }

  for (size_t i = 0; i < boot_memory_map.available_count; ++i) {
    struct memory_region region = available[i];
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
      for (j = 0; j < boot_memory_map.reserved_count; ++j) {
        struct memory_region reservation = reserved[j];
        uintptr_t reserved_end = reservation.offset + reservation.size;
        if (reservation.size != 0 && candidate < reserved_end &&
            reservation.offset < candidate_end) {
          start = reserved_end;
          break;
        }
      }
      if (j != boot_memory_map.reserved_count) {
        continue;
      }

      if (boot_memory_map.reserved_count == boot_memory_map.reserved_capacity) {
        return NULL;
      }
      reserved[boot_memory_map.reserved_count++] = (struct memory_region){
          .offset = candidate,
          .size = size,
          .flags = MEMORY_REGION_RESERVED,
      };
      return (void*)candidate;
    }
  }
  return NULL;
}
