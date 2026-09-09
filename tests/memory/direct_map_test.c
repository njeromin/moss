/* Run from the repository root:
 cc -std=c11 -Wall -Wextra -Werror -Ikernel/include -Icommon/include \
 -Ikernel/arch/riscv64/include tests/memory/direct_map_test.c \
 kernel/core/memory/direct_map.c common/memory.c -o /tmp/direct_map_test \
 && /tmp/direct_map_test
 */
#include <assert.h>
#include <core/memory/direct_map.h>
#include <stdio.h>

static struct {
  uintptr_t start, end;
} ranges[32];

static size_t count, budget;

uintptr_t arch_phys_to_virt(uintptr_t phys) {
  return HHDM_BASE + phys;
}

enum vmm_status vmm_map(struct vmm_space* space, uintptr_t virt, uintptr_t phys,
                        size_t size, uint8_t flags) {
  assert(space && virt == HHDM_BASE + phys);
  assert(flags == (PAGE_READ | PAGE_WRITE));
  assert(size && !(size % 4096) && !(phys % 4096));
  for (size_t i = 0; i < count; ++i) {
    assert(phys >= ranges[i].end || phys + size <= ranges[i].start);
  }
  if (count == budget) {
    return VMM_ERROR;
  }
  ranges[count].start = phys;
  ranges[count++].end = phys + size;
  return VMM_OK;
}

static int mapped(uintptr_t phys) {
  for (size_t i = 0; i < count; ++i) {
    if (phys >= ranges[i].start && phys < ranges[i].end) {
      return 1;
    }
  }
  return 0;
}

int main(void) {
  const uintptr_t base = 2ULL << 30;
  struct vmm_space space = {.root_phys = 4096};
  struct memory_region ram[] = {
      {.offset = base, .size = 2ULL << 30},
      {.offset = base + 4096, .size = 4ULL << 20},
      {.offset = base + (3ULL << 30) + 1, .size = 16383},
  };
  struct memory_region reserved[] = {
      {.offset = base + 8193, .size = 4096, .flags = MEMORY_REGION_NO_MAP},
      {.offset = base, .size = 4096, .flags = MEMORY_REGION_RESERVED},
      {.offset = base + 4097, .size = 8192, .flags = MEMORY_REGION_NO_MAP},
  };
  struct memory_map map = {.available = ram,
                           .available_count = 3,
                           .reserved = reserved,
                           .reserved_count = 3};
  budget = 32;
  assert(direct_map_init(&space, &map) == VMM_OK);
  assert(count == 3);
  assert(mapped(base));
  for (size_t i = 1; i <= 3; ++i) {
    assert(!mapped(base + i * 4096));
  }
  assert(mapped(base + 16384));
  assert(mapped(base + (1ULL << 30)));
  assert(!mapped(base - 4096));
  assert(!mapped(base + (2ULL << 30)));
  assert(!mapped(base + (3ULL << 30)));
  assert(mapped(base + (3ULL << 30) + 4096));
  assert(!mapped(base + (3ULL << 30) + 16384));
  count = 0;
  ram[1].flags = MEMORY_REGION_NO_MAP;
  assert(direct_map_init(&space, &map) == VMM_OK);
  assert(!mapped(base + (2ULL << 20)));
  assert(mapped(base + (6ULL << 20)));
  ram[1].flags = 0;
  count = 0;
  budget = 1;
  assert(direct_map_init(&space, &map) == VMM_ERROR);
  assert(count == 1);
  count = 0;
  reserved[0].offset = UINTPTR_MAX;
  assert(direct_map_init(&space, &map) == VMM_ERROR);
  assert(count == 0);
  reserved[0].size = 0;
  ram[0].offset = 128ULL << 30;
  assert(direct_map_init(&space, &map) == VMM_ERROR);
  assert(count == 0);
  assert(direct_map_init(NULL, &map) == VMM_ERROR);
  puts("direct map tests passed");
}
