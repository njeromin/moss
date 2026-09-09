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
  size_t words_count;
  size_t bitmap_size;
  uint8_t* bitmap;

  if (addr_start >= addr_end || (addr_start % PMM_FRAME_SIZE) != 0 ||
      (addr_end % PMM_FRAME_SIZE) != 0 ||
      pmm.region_count >= pmm.region_capacity) {
    return -1;
  }

  frame_count = (addr_end - addr_start) / PMM_FRAME_SIZE;
  words_count = (frame_count + 31) / 32;
  bitmap_size = words_count * sizeof(uint64_t);
  void* raw_bitmap = boot_alloc(bitmap_size, sizeof(uint64_t));
  if (raw_bitmap == NULL) {
    return -1;
  }
  bitmap = (uint8_t*)arch_phys_to_virt((uintptr_t)raw_bitmap);

  // Available frames start free. Reserved ranges are marked allocated below.
  for (size_t i = 0; i < bitmap_size; ++i) {
    bitmap[i] = 0;
  }

  // Pre-mark any padding frames in the last 64-bit word as reserved
  for (size_t frame = frame_count; frame < words_count * 32; ++frame) {
    set_frame_state(bitmap, frame, PMM_FRAME_RESERVED);
  }

  pmm.regions[pmm.region_count++] = (struct pmm_region){
      .base = addr_start,
      .frame_count = frame_count,
      .bitmap = bitmap,
      .cursor = 0};
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

static enum pmm_result find_free_run_in_range(const struct pmm_region* region,
                                             size_t requested_frames,
                                             size_t start_frame,
                                             size_t end_frame,
                                             size_t* first_frame) {
  if (region == NULL || requested_frames == 0 || start_frame >= end_frame ||
      end_frame > region->frame_count) {
    return PMM_ERROR_INVALID_INPUT;
  }

  const uint64_t* words = (const uint64_t*)region->bitmap;
  size_t start_word = start_frame / 32;
  size_t end_word = (end_frame + 31) / 32;

  size_t run_start = 0;
  size_t run_length = 0;

  for (size_t w = start_word; w < end_word; ++w) {
    size_t word_base_frame = w * 32;
    uint64_t word = words[w];

    // Compute occupied mask: bit 2k is 1 if frame k is allocated/reserved.
    uint64_t even = word & 0x5555555555555555ULL;
    uint64_t odd = (word >> 1) & 0x5555555555555555ULL;
    uint64_t occupied = even | odd;

    // Mask out frames before start_frame in the first word
    if (w == start_word && (start_frame % 32) != 0) {
      size_t offset = start_frame % 32;
      uint64_t mask = (1ULL << (2 * offset)) - 1;
      occupied |= (mask & 0x5555555555555555ULL);
    }

    // Mask out frames at or after end_frame in the last word
    if (w == (end_frame / 32) && (end_frame % 32) != 0) {
      size_t end_offset = end_frame % 32;
      uint64_t end_mask = ~((1ULL << (2 * end_offset)) - 1);
      occupied |= (end_mask & 0x5555555555555555ULL);
    }

    // Invert to get free_mask: bit 2k is 1 if frame k is free.
    uint64_t free_mask = (~occupied) & 0x5555555555555555ULL;

    // Fast path 1: Entire word (remaining frames) is occupied.
    if (free_mask == 0) {
      run_length = 0;
      continue;
    }

    // Fast path 2: Single-frame request.
    if (requested_frames == 1) {
      size_t k = (size_t)(__builtin_ctzl(free_mask) >> 1);
      size_t found = word_base_frame + k;
      if (found < end_frame) {
        *first_frame = found;
        return PMM_OK;
      }
      run_length = 0;
      continue;
    }

    size_t valid_in_word = 32;
    if (word_base_frame + 32 > end_frame) {
      valid_in_word = end_frame - word_base_frame;
    }
    size_t start_in_word = (w == start_word) ? (start_frame % 32) : 0;
    size_t unmasked_count = valid_in_word - start_in_word;

    // Fast path 3: All unmasked frames in this word are free.
    uint64_t expected_free = 0x5555555555555555ULL;
    if (start_in_word > 0) {
      expected_free &= ~((1ULL << (2 * start_in_word)) - 1);
    }
    if (valid_in_word < 32) {
      expected_free &= ((1ULL << (2 * valid_in_word)) - 1);
    }

    if (free_mask == expected_free) {
      if (run_length == 0) {
        run_start = word_base_frame + start_in_word;
      }
      run_length += unmasked_count;
      if (run_length >= requested_frames) {
        *first_frame = run_start;
        return PMM_OK;
      }
      continue;
    }

    // Multi-frame request across mixed word: scan bits
    size_t first_k = (size_t)(__builtin_ctzl(free_mask) >> 1);
    if (first_k > start_in_word) {
      run_length = 0;
    }

    for (size_t k = first_k; k < valid_in_word; ++k) {
      if ((free_mask & (1ULL << (2 * k))) != 0) {
        if (run_length == 0) {
          run_start = word_base_frame + k;
        }
        ++run_length;
        if (run_length == requested_frames) {
          *first_frame = run_start;
          return PMM_OK;
        }
      } else {
        run_length = 0;
      }
    }
  }

  return PMM_ERROR_NO_FREE_FRAME;
}

static enum pmm_result find_free_run(const struct pmm_region* region,
                                     size_t requested_frames,
                                     size_t* first_frame) {
  if (region == NULL || requested_frames == 0 ||
      requested_frames > region->frame_count) {
    return PMM_ERROR_INVALID_INPUT;
  }

  size_t cursor = region->cursor;
  if (cursor >= region->frame_count) {
    cursor = 0;
  }

  // First pass: search from cursor to the end of the region
  if (find_free_run_in_range(region, requested_frames, cursor,
                             region->frame_count, first_frame) == PMM_OK) {
    return PMM_OK;
  }

  // Second pass: wrap around from 0 to cursor
  if (cursor > 0) {
    size_t wrap_end = cursor + requested_frames - 1;
    if (wrap_end > region->frame_count) {
      wrap_end = region->frame_count;
    }
    if (find_free_run_in_range(region, requested_frames, 0, wrap_end,
                               first_frame) == PMM_OK) {
      return PMM_OK;
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

    region->cursor = first_frame + frames;
    if (region->cursor >= region->frame_count) {
      region->cursor = 0;
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

  if (first_frame < region->cursor) {
    region->cursor = first_frame;
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
