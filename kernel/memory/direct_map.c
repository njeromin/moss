#include <memory/direct_map.h>

#include <asm/paging_info.h>
#include <common/memory.h>

enum vmm_status direct_map_init(struct vmm_space* space,
                                const struct memory_map* map) {
  if (space == NULL || map == NULL ||
      (map->available_count && map->available == NULL) ||
      (map->reserved_count && map->reserved == NULL)) {
    return VMM_ERROR;
  }
  // Validate before allocating tables. Keep the existing 128 GiB HHDM window.
  for (size_t list = 0; list < 2; ++list) {
    const struct memory_region* regions = list ? map->reserved : map->available;
    size_t count = list ? map->reserved_count : map->available_count;
    for (size_t i = 0; i < count; ++i) {
      if (regions[i].size > UINTPTR_MAX - regions[i].offset ||
          (!list && regions[i].size &&
           regions[i].offset + regions[i].size > DIRECT_MAP_PHYS_LIMIT)) {
        return VMM_ERROR;
      }
    }
  }

  for (size_t i = 0; i < map->available_count; ++i) {
    struct memory_region ram = map->available[i];
    uintptr_t end = align_down(ram.offset + ram.size, PAGE_SIZE);

    if (ram.size == 0 || ram.offset >= end) {
      continue;
    }

    uintptr_t start = align_up(ram.offset, PAGE_SIZE);

    while (start < end) {
      uintptr_t stop = end;
      uintptr_t skip = start;

      // Subtract NO_MAP from both lists, including unsorted/overlapping ranges.
      // Exclude whole pages touched by a reservation; only map whole RAM pages.
      for (size_t list = 0; list < 2; ++list) {
        const struct memory_region* regions =
            list ? map->reserved : map->available;
        size_t count = list ? map->reserved_count : map->available_count;
        for (size_t j = 0; j < count; ++j) {
          struct memory_region r = regions[j];
          if (!(r.flags & MEMORY_REGION_NO_MAP) || r.size == 0) {
            continue;
          }
          uintptr_t lo = align_down(r.offset, PAGE_SIZE);
          if (lo >= end) {
            continue;
          }
          uintptr_t hi = r.offset + r.size;
          hi = hi >= end ? end : align_up(hi, PAGE_SIZE);
          if (hi <= start) {
            continue;
          }
          if (lo <= start) {
            if (hi > skip) {
              skip = hi;
            }
          } else if (lo < stop) {
            stop = lo;
          }
        }
      }

      if (skip > start) {
        start = skip;
        continue;
      }

      // Earlier RAM entries have already mapped their eligible pages. Skip
      // their full-page coverage so overlapping discovery entries aren't mapped twice.
      for (size_t j = 0; j < i; ++j) {
        struct memory_region previous = map->available[j];
        uintptr_t hi = align_down(previous.offset + previous.size, PAGE_SIZE);
        if (previous.offset >= hi) {
          continue;
        }
        uintptr_t lo = align_up(previous.offset, PAGE_SIZE);
        if (hi <= start || lo >= stop) {
          continue;
        }
        if (lo <= start) {
          if (hi > skip) {
            skip = hi;
          }
        } else {
          stop = lo;
        }
      }

      if (skip > start) {
        start = skip;
        continue;
      }

      if (vmm_map(space, arch_phys_to_virt(start), start, stop - start,
                  PAGE_READ | PAGE_WRITE) != VMM_OK) {
        return VMM_ERROR;
      }

      start = stop;
    }
  }

  return VMM_OK;
}
