#pragma once

#include <stddef.h>
#include <stdint.h>
#include <arch/memory/layout.h>

// TODO: Ensure this is the same size as the smallest page in the current architecture.
#define PMM_FRAME_SIZE 4096

enum pmm_frame_state {
  PMM_FRAME_FREE = 0,
  PMM_FRAME_ALLOCATED = 1,
  PMM_FRAME_RESERVED = 2,
  PMM_FRAME_RESERVED_RECLAIMABLE = 3,
};

struct pmm_region {
  uintptr_t base;
  size_t frame_count;
  uint8_t* bitmap;
  size_t cursor;
};

enum pmm_result {
  PMM_OK = 0,
  PMM_ERROR = -1,
  PMM_ERROR_INVALID_INPUT = -2,
  PMM_ERROR_NO_FREE_FRAME = -3,
  PMM_ERROR_ALREADY_FREE = -4,
  PMM_ERROR_INVALID_REGIONS = -5,
  PMM_ERROR_CANT_EMIT_REGION = -6,
};

struct pmm_ctx {
  struct pmm_region* regions;
  size_t region_count;
  size_t region_capacity;
};

// Boot-only initialization, completed before other harts allocate memory.
enum pmm_result pmm_init(void);

// Allocation/free are serialised and preserve the caller's interrupt state.
// Callers must remove mappings and finish shootdowns before freeing data frames.
enum pmm_result pmm_alloc(size_t frames, uintptr_t* out_ptr);
enum pmm_result pmm_free(uintptr_t base, size_t frames);
