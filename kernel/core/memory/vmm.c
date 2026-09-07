#include <core/memory/vmm.h>

#include <arch/memory/paging_info.h>
#include <core/memory/pmm.h>

static struct vmm_space kernel_space;

int vmm_init(void) {
  (void)arch_paging_create_space(&kernel_space.root_phys);
  return 0;
}

int vmm_map(struct vmm_space* space, uintptr_t virt, uintptr_t phys,
            size_t size, uint8_t flags) {
  if (space == NULL || (virt & PAGE_MASK) != 0 || (phys & PAGE_MASK) != 0) {
    return -1;
  }

  for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
    int res =
        arch_paging_map(space->root_phys, virt + offset, phys + offset, flags);
    if (res != PAGING_OK) {
      // (Optional: unmap previously mapped pages on failure)
      return res;
    }
  }

  return 0;
}

int vmm_unmap(struct vmm_space* space, uintptr_t virt, size_t size) {
  if (space == NULL || size == 0) {
    return -1;
  }

  // Both virt and size must be page-aligned
  if ((virt & PAGE_MASK) != 0 || (size & PAGE_MASK) != 0) {
    return -1;
  }

  // Overflow check (virt + size wrapping around 64-bit integer)
  if (virt + size < virt) {
    return -1;
  }

  for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
    int res = arch_paging_unmap(space->root_phys, virt + offset);

    // Depending on your policy:
    // If you want POSIX munmap behavior (allowing holes/unmapped pages in range):
    // if (res != PAGING_OK && res != PAGING_ERR_NOT_MAPPED) return res;

    // Strict behavior: fail if any page in the range wasn't mapped
    if (res != PAGING_OK) {
      return res;
    }
  }

  return 0;
}

int vmm_translate(const struct vmm_space* space, uintptr_t virt,
                  uintptr_t* phys) {
  if (space == NULL || phys == NULL || space->root_phys == 0) {
    return -1;
  }

  // Query the hardware page table; we pass NULL for permissions since we only want phys
  int res = arch_paging_query(space->root_phys, virt, phys, NULL);
  if (res != PAGING_OK) {
    return res;
  }

  return 0;
}
