#include <core/memory/memory_map.h>

#include <arch/memory/layout.h>
#include <common/memory.h>
#include <core/console.h>
#include <core/panic.h>
#include <stdbool.h>

#define _CONSOLE_LOG_SUBJECT "memory_map"

static struct memory_region available_regions[MAX_MEMORY_REGIONS];
static struct memory_region reserved_regions[MAX_MEMORY_REGIONS];

static struct memory_map global_memory_map = {
    .available = available_regions,
    .available_capacity = MAX_MEMORY_REGIONS,
    .available_count = 0,
    .reserved = reserved_regions,
    .reserved_capacity = MAX_MEMORY_REGIONS,
    .reserved_count = 0,
    .min_phys_addr = 0,
    .max_phys_addr = 0,
};

static uintptr_t map_find_free_ram(const struct memory_map* map, size_t size,
                                   size_t align,
                                   const struct memory_region* pending) {
  if (size == 0 || align == 0) {
    return 0;
  }

  for (size_t i = 0; i < map->available_count; ++i) {
    struct memory_region avail = map->available[i];
    if (avail.flags & (MEMORY_REGION_RESERVED | MEMORY_REGION_NO_MAP)) {
      continue;
    }

    uintptr_t start = avail.offset;
    if (start < kernel_physical_end) {
      start = kernel_physical_end;
    }

    uintptr_t end = avail.offset + avail.size;
    if (start >= end) {
      continue;
    }

    while (start < end && start <= UINTPTR_MAX - (align - 1)) {
      uintptr_t candidate = align_up(start, align);
      if (candidate >= end || size > end - candidate) {
        break;
      }
      uintptr_t candidate_end = candidate + size;

      bool conflict = false;

      if (pending != NULL && pending->size != 0) {
        uintptr_t pending_end = pending->offset + pending->size;
        if (candidate < pending_end && pending->offset < candidate_end) {
          if (pending_end > start) {
            start = pending_end;
          } else {
            break;
          }
          conflict = true;
        }
      }

      if (conflict) {
        continue;
      }

      for (size_t j = 0; j < map->reserved_count; ++j) {
        struct memory_region res = map->reserved[j];
        uintptr_t res_end = res.offset + res.size;
        if (res.size != 0 && candidate < res_end &&
            res.offset < candidate_end) {
          if (res_end > start) {
            start = res_end;
          } else {
            break;
          }
          conflict = true;
          break;
        }
      }

      if (!conflict) {
        return candidate;
      }
    }
  }
  return 0;
}

static int map_grow_reserved(struct memory_map* map,
                             const struct memory_region* pending) {
  if (map->available_count == 0) {
    return -1;
  }

  size_t old_cap = map->reserved_capacity;
  size_t new_cap = (old_cap < 4) ? 4 : old_cap * 2;
  size_t alloc_size = align_up(new_cap * sizeof(struct memory_region), 4096);

  uintptr_t phys = map_find_free_ram(map, alloc_size, 4096, pending);
  if (phys == 0) {
    return -1;
  }

  struct memory_region* new_array =
      (struct memory_region*)arch_phys_to_virt(phys);
  memory_set(new_array, 0, new_cap * sizeof(struct memory_region));
  memory_copy(new_array, map->reserved,
              map->reserved_count * sizeof(struct memory_region));

  new_array[map->reserved_count++] = (struct memory_region){
      .offset = phys,
      .size = alloc_size,
      .flags = MEMORY_REGION_RESERVED,
  };

  if (phys < map->min_phys_addr) {
    map->min_phys_addr = phys;
  }
  if (phys + alloc_size > map->max_phys_addr) {
    map->max_phys_addr = phys + alloc_size;
  }

  map->reserved = new_array;
  map->reserved_capacity = new_cap;
  return 0;
}

int memory_map_init(const struct device_discovery_info device_discovery) {
  int res = arch_get_memory_map(device_discovery, &global_memory_map);
  if (res != ARCH_MEMORY_MAP_OK) {
    switch (res) {
      case ARCH_MEMORY_MAP_INVALID:
        kpanic("Encountered invalid map while getting memory map.\n");
        break;
      case ARCH_MEMORY_MAP_NO_SPACE:
        kpanic("No space while getting memory map.\n");
        break;
      default:
        kpanic("Unexpected error while getting memory map.\n");
        break;
    }
    return -1;
  }

  console_log(_CONSOLE_LOG_SUBJECT, "Found %zu available memory regions.\n",
              global_memory_map.available_count);
  for (size_t i = 0; i < global_memory_map.available_count; i++) {
    struct memory_region region = global_memory_map.available[i];
    console_log(_CONSOLE_LOG_SUBJECT, "-> %p (%zu bytes)\n",
                (void*)region.offset, region.size);
  }

  console_log(_CONSOLE_LOG_SUBJECT, "Found %zu reserved memory regions.\n",
              global_memory_map.reserved_count);
  for (size_t i = 0; i < global_memory_map.reserved_count; i++) {
    struct memory_region region = global_memory_map.reserved[i];
    console_log(_CONSOLE_LOG_SUBJECT, "-> %p (%zu bytes)\n",
                (void*)region.offset, region.size);
  }

  console_log(_CONSOLE_LOG_SUBJECT, "Lowest address found:  %p.\n",
              (void*)global_memory_map.min_phys_addr);
  console_log(_CONSOLE_LOG_SUBJECT, "Highest address found: %p.\n",
              (void*)global_memory_map.max_phys_addr);

  return 0;
}

const struct memory_map* memory_map_get(void) {
  return &global_memory_map;
}

int memory_map_add_reserved(struct memory_region region) {
  if (global_memory_map.reserved_count >= global_memory_map.reserved_capacity) {
    if (map_grow_reserved(&global_memory_map, &region) != 0) {
      return -1;
    }
  }
  global_memory_map.reserved[global_memory_map.reserved_count++] = region;
  return 0;
}
