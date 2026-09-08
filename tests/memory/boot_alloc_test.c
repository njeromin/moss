/* Run: cc -std=c11 -Wall -Wextra -Ikernel/include -Icommon/include
   tests/memory/boot_alloc_test.c kernel/core/memory/allocation/boot_alloc.c
   kernel/core/memory/pmm.c common/memory.c -o /tmp/boot_alloc_test
   Then run /tmp/boot_alloc_test. */
#include <assert.h>
#include <core/memory/allocation/boot_alloc.h>
#include <core/memory/pmm.h>
#include <stdlib.h>

void kpanic(const char* format, ...) {
  (void)format;
  abort();
}

uintptr_t arch_phys_to_virt(uintptr_t phys) {
  return phys;
}

static _Alignas(4096) unsigned char ram[4096 * 32];

int main(void) {
  uintptr_t base = (uintptr_t)ram;
  struct memory_region memory[] = {{.offset = base, .size = sizeof(ram)}};
  // Deliberately unsorted and overlapping reservations.
  struct memory_region reserved[] = {
      {.offset = base + 64, .size = 64},
      {.offset = base, .size = 80},
  };
  struct memory_map map = {
      .available = memory,
      .available_count = 1,
      .available_capacity = 1,
      .reserved = reserved,
      .reserved_count = 2,
      .reserved_capacity = 2,
  };
  assert(boot_alloc(1, 1) == NULL);
  boot_alloc_init(&map, base, base + sizeof(ram));
  // Mutating the discovery arrays must not affect the allocator's copy.
  reserved[0].size = 0;
  assert(boot_alloc(16, 64) == ram + 128);
  assert(boot_alloc(16, 64) == ram + 192);
  assert(boot_alloc(0, 1) == NULL);
  assert(boot_alloc(1, 0) == NULL);
  assert(boot_alloc(1, 3) == NULL);
  assert(boot_alloc(SIZE_MAX, 8) == NULL);

  map.reserved_count = 0;
  boot_alloc_init(&map, base + 256, base + 272);
  assert(boot_alloc(16, 16) == ram + 256);
  assert(boot_alloc(1, 1) == NULL);

  // A full reservation array must fail without handing out untracked RAM.
  boot_alloc_init(&map, base, base + sizeof(ram));
  size_t capacity = boot_alloc_memory_map()->reserved_capacity;
  for (size_t i = 0; i < capacity; ++i) {
    assert(boot_alloc(1, 1) != NULL);
  }
  assert(boot_alloc(1, 1) == NULL);

  memory[0] = (struct memory_region){.offset = UINTPTR_MAX - 31, .size = 31};
  boot_alloc_init(&map, UINTPTR_MAX - 31, UINTPTR_MAX);
  assert(boot_alloc(1, 64) == NULL);

  // Two RAM regions: reserve all early metadata in both PMM bitmaps.
  struct memory_region regions[] = {
      {.offset = base, .size = 4096},
      {.offset = base + 8192, .size = sizeof(ram) - 8192},
  };
  map.available = regions;
  map.available_count = map.available_capacity = 2;
  boot_alloc_init(&map, base, base + sizeof(ram));
  assert(pmm_init() == PMM_OK);
  assert(boot_alloc(1, 1) == NULL);
  const struct memory_map* owned = boot_alloc_memory_map();
  for (size_t i = 0; i < owned->reserved_count; ++i) {
    uintptr_t page = owned->reserved[i].offset & ~(uintptr_t)4095;
    assert(pmm_free(page, 1) == PMM_ERROR_ALREADY_FREE);
  }
  uintptr_t frame;
  assert(pmm_alloc(1, &frame) == PMM_OK);
  assert(frame == base + 8192);
  assert(pmm_free(frame, 1) == PMM_OK);
  assert(pmm_alloc(30, &frame) == PMM_OK);
  assert(frame == base + 8192);
  assert(pmm_alloc(1, &frame) == PMM_ERROR_NO_FREE_FRAME);
  return 0;
}
