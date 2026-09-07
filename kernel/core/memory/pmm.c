#include <core/memory/pmm.h>
#include <stdint.h>

#include "common/memory.h"
#include "core/memory/allocation/boot_alloc.h"
#include "core/memory/regions.h"

static struct pmm_ctx pmm;

static int emit_pmm_region(uintptr_t addr_start, uintptr_t addr_end) {
  size_t frame_count;
  size_t bitmap_size;
  uint8_t* bitmap;

  if (addr_start >= addr_end || (addr_start % PMM_FRAME_SIZE) != 0 ||
      (addr_end % PMM_FRAME_SIZE) != 0 ||
      pmm.region_count >= pmm.region_capacity) {
    return -1;
  }

  frame_count = (addr_end - addr_start) / PMM_FRAME_SIZE;
  bitmap_size = (frame_count + 7) / 8;
  bitmap = boot_alloc(pmm.boot_allocator, bitmap_size, 1);
  if (bitmap == 0) {
    return -1;
  }

  // Available frames start free. Reserved ranges are marked allocated below.
  for (size_t i = 0; i < bitmap_size; ++i) {
    bitmap[i] = 0;
  }

  pmm.regions[pmm.region_count++] = (struct pmm_region){
      .base = addr_start, .frame_count = frame_count, .bitmap = bitmap};
  return 0;
}

static int pmm_region_reserve(struct pmm_region* region, uintptr_t start,
                              uintptr_t end) {
  uintptr_t region_end;
  uintptr_t overlap_start;
  uintptr_t overlap_end;
  size_t first_frame;
  size_t last_frame;

  if (region == 0 || start >= end ||
      region->frame_count > (UINTPTR_MAX - region->base) / PMM_FRAME_SIZE) {
    return -1;
  }
  region_end = region->base + region->frame_count * PMM_FRAME_SIZE;
  overlap_start = start > region->base ? start : region->base;
  overlap_end = end < region_end ? end : region_end;
  if (overlap_start >= overlap_end) {
    return 0;
  }

  first_frame = (overlap_start - region->base) / PMM_FRAME_SIZE;
  last_frame = (overlap_end - region->base) / PMM_FRAME_SIZE;
  for (size_t frame = first_frame; frame < last_frame; ++frame) {
    region->bitmap[frame / 8] |= (uint8_t)(1u << (frame % 8));
  }
  return 0;
}

int pmm_init(const struct memory_map* memory_map,
             struct boot_allocator* boot_allocator) {
  pmm.regions = NULL;
  pmm.region_count = 0;
  pmm.region_capacity = 0;
  pmm.boot_allocator = boot_allocator;

  size_t region_capacity;

  if (memory_map == 0 || pmm.boot_allocator == 0) {
    return -1;
  }
  region_capacity = memory_map->available_count;
  if (region_capacity == 0) {
    return 0;
  }
  if (region_capacity > SIZE_MAX / sizeof(*pmm.regions)) {
    return -1;
  }

  pmm.regions =
      boot_alloc(pmm.boot_allocator, sizeof(*pmm.regions) * region_capacity,
                 _Alignof(struct pmm_region));
  if (pmm.regions == 0) {
    return -1;
  }
  pmm.region_count = 0;
  pmm.region_capacity = region_capacity;

  // Keep one PMM region per available RAM range. Reserved frames are marked
  // allocated in the region bitmap below.
  for (size_t i = 0; i < memory_map->available_count; ++i) {
    struct memory_region available = memory_map->available[i];
    uintptr_t available_end;
    uintptr_t start;
    uintptr_t end;

    if (available.size > UINTPTR_MAX - available.offset) {
      return -1;
    }
    available_end = available.offset + available.size;
    start = align_up(available.offset, PMM_FRAME_SIZE);
    end = align_down(available_end, PMM_FRAME_SIZE);
    if (start >= end) {
      continue;
    }
    if (emit_pmm_region(start, end) != 0) {
      return -1;
    }

    struct pmm_region* region = &pmm.regions[pmm.region_count - 1];
    for (size_t j = 0; j < memory_map->reserved_count; ++j) {
      struct memory_region reserved = memory_map->reserved[j];
      uintptr_t reserved_end;
      uintptr_t reserved_start;
      if (reserved.size > UINTPTR_MAX - reserved.offset) {
        return -1;
      }
      reserved_end = reserved.offset + reserved.size;
      reserved_start = align_down(reserved.offset, PMM_FRAME_SIZE);
      reserved_end = align_up(reserved_end, PMM_FRAME_SIZE);
      if (pmm_region_reserve(region, reserved_start, reserved_end) != 0) {
        return -1;
      }
    }
  }
  return 0;
}

static int find_free_run(const struct pmm_region* region,
                         size_t requested_frames, size_t* first_frame) {
  size_t run_start = 0;
  size_t run_length = 0;

  if (region == 0 || requested_frames == 0) {
    return -1;
  }

  for (size_t frame = 0; frame < region->frame_count; ++frame) {
    size_t byte = frame / 8;
    uint8_t mask = (uint8_t)(1u << (frame % 8));
    int allocated = (region->bitmap[byte] & mask) != 0;

    if (!allocated) {
      if (run_length == 0) {
        run_start = frame;
      }

      ++run_length;

      if (run_length == requested_frames) {
        *first_frame = run_start;
        return 0;
      }
    } else {
      run_length = 0;
    }
  }

  return -1;
}

int pmm_alloc(size_t frames, uintptr_t* out_ptr) {
  if (out_ptr == 0 || frames == 0) {
    return -1;
  }

  *out_ptr = 0;

  for (size_t i = 0; i < pmm.region_count; i++) {
    struct pmm_region* region = &pmm.regions[i];
    size_t first_frame;

    if (find_free_run(region, frames, &first_frame) != 0) {
      continue;
    }

    for (size_t frame = first_frame; frame < first_frame + frames; ++frame) {
      region->bitmap[frame / 8] |= (uint8_t)(1u << (frame % 8));
    }

    *out_ptr = region->base + first_frame * PMM_FRAME_SIZE;
    return 0;
  }

  return -1;
}

static struct pmm_region* find_region(uintptr_t base, size_t frames) {
  uintptr_t size;
  uintptr_t end;

  if (frames == 0 || frames > (UINTPTR_MAX - base) / PMM_FRAME_SIZE) {
    return 0;
  }

  size = frames * PMM_FRAME_SIZE;
  end = base + size;

  for (size_t i = 0; i < pmm.region_count; ++i) {
    struct pmm_region* region = &pmm.regions[i];

    if (region->frame_count > (UINTPTR_MAX - region->base) / PMM_FRAME_SIZE) {
      continue;
    }

    uintptr_t region_end = region->base + region->frame_count * PMM_FRAME_SIZE;

    if (base >= region->base && end <= region_end) {
      return region;
    }
  }

  return 0;
}

int pmm_free(uintptr_t base, size_t frames) {
  if ((base % PMM_FRAME_SIZE) != 0) {
    return -1;
  }

  struct pmm_region* region = find_region(base, frames);
  if (region == 0) {
    return -1;
  }

  size_t first_frame = (base - region->base) / PMM_FRAME_SIZE;

  for (size_t i = 0; i < frames; ++i) {
    size_t frame = first_frame + i;
    size_t byte = frame / 8;
    uint8_t mask = (uint8_t)(1u << (frame % 8));

    if ((region->bitmap[byte] & mask) == 0) {
      return -1;  // Already free
    }
  }

  for (size_t i = 0; i < frames; ++i) {
    size_t frame = first_frame + i;
    region->bitmap[frame / 8] &= (uint8_t)~(1u << (frame % 8));
  }

  return 0;
}
