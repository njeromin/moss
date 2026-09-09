#include <core/memory/pmm.h>
#include <stdint.h>

#include "common/memory.h"
#include "core/memory/allocation/boot_alloc.h"
#include "core/memory/regions.h"
#include <arch/memory/layout.h>

#include <core/sync/spinlock.h>

static struct pmm_ctx pmm;
static struct spinlock pmm_lock;

static enum pmm_frame_state frame_state(const uint8_t* bitmap, size_t frame) {
  size_t bit = frame * 2;
  return (enum pmm_frame_state)((bitmap[bit / 8] >> (bit % 8)) & 3u);
}

static void set_frame_state(uint8_t* bitmap, size_t frame,
                            enum pmm_frame_state state) {
  size_t bit = frame * 2;
  uint8_t shift = (uint8_t)(bit % 8);
  uint8_t mask = (uint8_t)(3u << shift);
  bitmap[bit / 8] =
      (uint8_t)((bitmap[bit / 8] & (uint8_t)~mask) | ((uint8_t)state << shift));
}

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
  bitmap_size = (frame_count + 3) / 4;
  void* raw_bitmap = boot_alloc(bitmap_size, 1);
  if (raw_bitmap == NULL) {
    return -1;
  }
  bitmap = (uint8_t*)arch_phys_to_virt((uintptr_t)raw_bitmap);

  // Available frames start free. Reserved ranges are marked allocated below.
  for (size_t i = 0; i < bitmap_size; ++i) {
    bitmap[i] = 0;
  }

  pmm.regions[pmm.region_count++] = (struct pmm_region){
      .base = addr_start, .frame_count = frame_count, .bitmap = bitmap};
  return 0;
}

static int pmm_region_reserve(struct pmm_region* region, uintptr_t start,
                              uintptr_t end, enum pmm_frame_state state) {
  uintptr_t region_end;
  uintptr_t overlap_start;
  uintptr_t overlap_end;
  size_t first_frame;
  size_t last_frame;

  if (region == 0 || start >= end ||
      (state != PMM_FRAME_RESERVED &&
       state != PMM_FRAME_RESERVED_RECLAIMABLE) ||
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
    if (frame_state(region->bitmap, frame) != PMM_FRAME_RESERVED) {
      set_frame_state(region->bitmap, frame, state);
    }
  }
  return 0;
}

enum pmm_result pmm_init(void) {
  const struct memory_map* memory_map = boot_alloc_memory_map();
  pmm.regions = NULL;
  pmm.region_count = 0;
  pmm.region_capacity = 0;

  size_t region_capacity;

  if (memory_map == 0) {
    return PMM_ERROR_INVALID_INPUT;
  }
  region_capacity = memory_map->available_count;
  if (region_capacity == 0) {
    return PMM_ERROR_INVALID_REGIONS;
  }
  if (region_capacity > SIZE_MAX / sizeof(*pmm.regions)) {
    return PMM_ERROR_INVALID_REGIONS;
  }

  void* raw_regions = boot_alloc(sizeof(*pmm.regions) * region_capacity,
                                 _Alignof(struct pmm_region));
  if (raw_regions == NULL) {
    return PMM_ERROR_INVALID_REGIONS;
  }
  pmm.regions = (struct pmm_region*)arch_phys_to_virt((uintptr_t)raw_regions);
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
      return PMM_ERROR_INVALID_INPUT;
    }
    available_end = available.offset + available.size;
    if (available.offset > UINTPTR_MAX - (PMM_FRAME_SIZE - 1)) {
      continue;
    }
    start = align_up(available.offset, PMM_FRAME_SIZE);
    end = align_down(available_end, PMM_FRAME_SIZE);
    if (start >= end) {
      continue;
    }
    if (emit_pmm_region(start, end) != 0) {
      return PMM_ERROR_CANT_EMIT_REGION;
    }
  }

  // All metadata must be reserved before any bitmap consumes the map.
  boot_alloc_finish();
  for (size_t i = 0; i < pmm.region_count; ++i) {
    struct pmm_region* region = &pmm.regions[i];
    for (size_t j = 0; j < memory_map->reserved_count; ++j) {
      struct memory_region reserved = memory_map->reserved[j];
      uintptr_t reserved_end;
      uintptr_t reserved_start;
      if (reserved.size > UINTPTR_MAX - reserved.offset) {
        return PMM_ERROR_INVALID_INPUT;
      }
      if (reserved.size == 0) {
        continue;
      }
      reserved_end = reserved.offset + reserved.size;
      if (reserved_end > UINTPTR_MAX - (PMM_FRAME_SIZE - 1)) {
        return PMM_ERROR_INVALID_INPUT;
      }
      reserved_start = align_down(reserved.offset, PMM_FRAME_SIZE);
      reserved_end = align_up(reserved_end, PMM_FRAME_SIZE);
      enum pmm_frame_state state = (reserved.flags & MEMORY_REGION_REUSABLE)
                                       ? PMM_FRAME_RESERVED_RECLAIMABLE
                                       : PMM_FRAME_RESERVED;
      if (pmm_region_reserve(region, reserved_start, reserved_end, state) !=
          0) {
        return PMM_ERROR_CANT_EMIT_REGION;  // Probably the wrong error?
      }
    }
  }
  return 0;
}

static enum pmm_result find_free_run(const struct pmm_region* region,
                                     size_t requested_frames,
                                     size_t* first_frame) {
  size_t run_start = 0;
  size_t run_length = 0;

  if (region == 0 || requested_frames == 0) {
    return PMM_ERROR_INVALID_INPUT;
  }

  for (size_t frame = 0; frame < region->frame_count; ++frame) {
    int allocated = frame_state(region->bitmap, frame) != PMM_FRAME_FREE;

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

  return PMM_ERROR_NO_FREE_FRAME;
}

static enum pmm_result pmm_alloc_locked(size_t frames, uintptr_t* out_ptr) {
  if (out_ptr == 0 || frames == 0) {
    return PMM_ERROR_INVALID_INPUT;
  }

  *out_ptr = 0;

  for (size_t i = 0; i < pmm.region_count; i++) {
    struct pmm_region* region = &pmm.regions[i];
    size_t first_frame;

    if (find_free_run(region, frames, &first_frame) != PMM_OK) {
      continue;
    }

    for (size_t frame = first_frame; frame < first_frame + frames; ++frame) {
      set_frame_state(region->bitmap, frame, PMM_FRAME_ALLOCATED);
    }

    *out_ptr = region->base + first_frame * PMM_FRAME_SIZE;
    return PMM_OK;
  }

  return PMM_ERROR_NO_FREE_FRAME;
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

static enum pmm_result pmm_free_locked(uintptr_t base, size_t frames) {
  if ((base % PMM_FRAME_SIZE) != 0) {
    return PMM_ERROR_INVALID_INPUT;
  }

  struct pmm_region* region = find_region(base, frames);
  if (region == 0) {
    return PMM_ERROR_INVALID_INPUT;
  }

  size_t first_frame = (base - region->base) / PMM_FRAME_SIZE;

  for (size_t i = 0; i < frames; ++i) {
    size_t frame = first_frame + i;
    if (frame_state(region->bitmap, frame) != PMM_FRAME_ALLOCATED) {
      return PMM_ERROR_ALREADY_FREE;
    }
  }

  for (size_t i = 0; i < frames; ++i) {
    size_t frame = first_frame + i;
    set_frame_state(region->bitmap, frame, PMM_FRAME_FREE);
  }

  return PMM_OK;
}

enum pmm_result pmm_alloc(size_t frames, uintptr_t* out_ptr) {
  bool irq = spin_lock_irqsave(&pmm_lock);
  enum pmm_result result = pmm_alloc_locked(frames, out_ptr);
  spin_unlock_irqrestore(&pmm_lock, irq);
  return result;
}

enum pmm_result pmm_free(uintptr_t base, size_t frames) {
  bool irq = spin_lock_irqsave(&pmm_lock);
  enum pmm_result result = pmm_free_locked(base, frames);
  spin_unlock_irqrestore(&pmm_lock, irq);
  return result;
}
