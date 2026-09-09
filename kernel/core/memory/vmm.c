#include <core/memory/vmm.h>

#include <arch/memory/layout.h>
#include <arch/memory/paging_info.h>
#include <common/memory.h>
#include <core/memory/pmm.h>

static struct vmm_space kernel_space;

enum vmm_status vmm_init(void) {
  if (arch_paging_create_space(&kernel_space.root_phys) != 0) {
    return VMM_ERROR;
  }

  uintptr_t phys_start = kernel_physical_start;
  uintptr_t phys_end = kernel_physical_end;
  size_t kernel_size = align_up(phys_end - phys_start, PAGE_SIZE);
  uintptr_t virt_start = arch_kernel_phys_to_virt(phys_start);

  int err = vmm_map(&kernel_space, virt_start, phys_start, kernel_size,
                    PAGE_READ | PAGE_WRITE | PAGE_EXEC);
  if (err != 0) {
    return err;
  }

  arch_paging_switch_space(kernel_space.root_phys, 0);

  return VMM_OK;
}

struct vmm_space* vmm_get_kernel_space(void) {
  return &kernel_space;
}

enum vmm_status vmm_map(struct vmm_space* space, uintptr_t virt, uintptr_t phys,
                        size_t size, uint8_t flags) {
  if (space == NULL || (virt & PAGE_MASK) != 0 || (phys & PAGE_MASK) != 0) {
    return VMM_ERROR;
  }

  for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
    int res =
        arch_paging_map(space->root_phys, virt + offset, phys + offset, flags);
    if (res != PAGING_OK) {
      return res;
    }
  }

  return VMM_OK;
}

enum vmm_status vmm_unmap(struct vmm_space* space, uintptr_t virt,
                          size_t size) {
  if (space == NULL || size == 0) {
    return VMM_ERROR;
  }

  // Both virt and size must be page-aligned
  if ((virt & PAGE_MASK) != 0 || (size & PAGE_MASK) != 0) {
    return VMM_ERROR;
  }

  // Overflow check (virt + size wrapping around 64-bit integer)
  if (virt + size < virt) {
    return VMM_ERROR;
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

  return VMM_OK;
}

enum vmm_status vmm_translate(const struct vmm_space* space, uintptr_t virt,
                              uintptr_t* phys) {
  if (space == NULL || phys == NULL || space->root_phys == 0) {
    return VMM_ERROR;
  }

  // Query the hardware page table; we pass NULL for permissions since we only want phys
  int res = arch_paging_query(space->root_phys, virt, phys, NULL);
  if (res != PAGING_OK) {
    return res;
  }

  return VMM_OK;
}
