// Run in the separate paging_tests.elf image with the run_paging_tests target.
#include <arch/memory/layout.h>
#include <arch/memory/paging.h>
#include <arch/memory/paging_info.h>
#include <core/console.h>
#include <core/memory/pmm.h>
#include <core/memory/vmm.h>
#include <core/panic.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition))                                                          \
      kpanic("Paging test failed: %s", #condition);                            \
  } while (0)
#define MIB (1ULL << 20)
#define GIB (1ULL << 30)

static int level_at(uintptr_t root, uintptr_t virt) {
  uint64_t* table = (uint64_t*)arch_phys_to_virt(root);
  for (int level = 2; level >= 0; --level) {
    uint64_t pte = table[(virt >> (12 + 9 * level)) & 511];
    if (!(pte & 1)) {
      return -1;
    }
    if (pte & 14) {
      return level;
    }
    table = (uint64_t*)arch_phys_to_virt((pte >> 10) << 12);
  }
  return -1;
}

void paging_test(void) {
  uintptr_t root, phys;
  unsigned permissions;
  struct page_status status;
  uintptr_t master = vmm_get_kernel_space()->root_phys;
  uintptr_t shared = HHDM_BASE + 16 * GIB;
  CHECK(arch_paging_map_range(master, shared, 16 * GIB, GIB,
                              PAGE_READ | PAGE_WRITE) == PAGING_OK);
  CHECK(level_at(master, shared) == 2);
  CHECK(arch_paging_create_space(&root) == PAGING_OK);
  CHECK(level_at(master, shared) == 1);
  CHECK(arch_paging_unmap(root, shared + 4096) == PAGING_OK);
  CHECK(level_at(master, shared + 4096) == -1);
  CHECK(level_at(root, shared) == 0);
  CHECK(arch_paging_query(master, shared + 8192, &phys, NULL) == PAGING_OK);
  CHECK(phys == 16 * GIB + 8192);
  // Unaligned ends surround a 1 GiB leaf and a 2 MiB leaf.
  CHECK(arch_paging_map_range(root, GIB - 4096, 3 * GIB - 4096,
                              GIB + 2 * MIB + 8192,
                              PAGE_READ | PAGE_WRITE) == PAGING_OK);
  CHECK(level_at(root, GIB - 4096) == 0);
  CHECK(level_at(root, GIB) == 2);
  CHECK(level_at(root, 2 * GIB) == 1);
  CHECK(level_at(root, 2 * GIB + 2 * MIB) == 0);
  CHECK(level_at(root, GIB - 8192) == -1);
  CHECK(level_at(root, 2 * GIB + 2 * MIB + 4096) == -1);
  CHECK(arch_paging_query(root, GIB + 123, &phys, &permissions) == PAGING_OK);
  CHECK(phys == 3 * GIB + 123 && permissions == (PAGE_READ | PAGE_WRITE));
  CHECK(arch_paging_map(root, GIB, 0, PAGE_READ) == PAGING_ERR_ALREADY_MAPPED);

  // Split twice to remove one 4 KiB page from a 1 GiB mapping.
  uintptr_t hole = GIB + 2 * MIB + 4096;
  CHECK(arch_paging_unmap_ext(root, hole, &status, &phys) == PAGING_OK);
  CHECK(phys == 3 * GIB + 2 * MIB + 4096 && status.accessed && status.dirty);
  CHECK(level_at(root, hole) == -1);
  CHECK(level_at(root, hole - 4096) == 0);
  CHECK(level_at(root, GIB) == 1);
  CHECK(arch_paging_query(root, hole + 4096, &phys, &permissions) == PAGING_OK);
  CHECK(phys == 3 * GIB + 2 * MIB + 8192 &&
        permissions == (PAGE_READ | PAGE_WRITE));
  CHECK(arch_paging_unmap(root, hole) == PAGING_ERR_NOT_MAPPED);
  CHECK(arch_paging_map(root, hole, 6 * GIB, PAGE_READ) == PAGING_OK);
  CHECK(arch_paging_query(root, hole, &phys, &permissions) == PAGING_OK);
  CHECK(phys == 6 * GIB && permissions == PAGE_READ);

  // Independently split a 2 MiB leaf without changing neighboring pages.
  CHECK(arch_paging_unmap(root, 2 * GIB + 4096) == PAGING_OK);
  CHECK(level_at(root, 2 * GIB) == 0);
  CHECK(level_at(root, 2 * GIB + 4096) == -1);
  CHECK(level_at(root, 2 * GIB + 8192) == 0);
  arch_paging_destroy_space(root);

  CHECK(arch_paging_create_space(&root) == PAGING_OK);
  // Existing branches force smaller leaves even for an aligned large request.
  CHECK(arch_paging_map(root, 8 * GIB + 2 * MIB, 0, PAGE_READ) == PAGING_OK);
  CHECK(arch_paging_map_range(root, 8 * GIB, 0, 2 * MIB, PAGE_READ) ==
        PAGING_OK);
  CHECK(level_at(root, 8 * GIB) == 1);
  // VA/PA alignment mismatch prevents large leaves.
  CHECK(arch_paging_map_range(root, 9 * GIB, 4096, 2 * MIB, PAGE_READ) ==
        PAGING_OK);
  CHECK(level_at(root, 9 * GIB) == 0);
  // Exercise hardware translation caches while splitting an active mapping.
  uintptr_t frame;
  CHECK(pmm_alloc(1, &frame) == PMM_OK);
  uintptr_t alias = 32 * GIB + (frame & (GIB - 1));
  CHECK(arch_paging_map_range(root, 32 * GIB, frame & ~(GIB - 1), GIB,
                              PAGE_READ | PAGE_WRITE) == PAGING_OK);
  volatile uint64_t* direct = (volatile uint64_t*)arch_phys_to_virt(frame);
  *direct = 0x12345678;
  arch_paging_switch_space(root, 0);
  CHECK(*(volatile uint64_t*)alias == 0x12345678);
  CHECK(arch_paging_unmap(root, alias + 4096) == PAGING_OK);
  *(volatile uint64_t*)alias = 0x87654321;
  CHECK(*direct == 0x87654321);
  CHECK(level_at(root, alias + 4096) == -1);
  arch_paging_switch_space(master, 0);
  CHECK(pmm_free(frame, 1) == PMM_OK);
  arch_paging_destroy_space(root);
  console_log("paging_test",
              "Large-page mapping and partial unmap tests passed.\n");
  extern void paging_smp_test(void);
  paging_smp_test();
}
